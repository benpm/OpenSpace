# Optimizations on `project/video_overlays`

Performance work carried out on this branch relative to `master`, written up so each
change can be reviewed on its own. Every number below was measured on this branch against
the immediately preceding commit — they are not estimates.

The work was driven by a concrete failure: the Denver Museum of Nature and Science
"Digital Earth — Energy I" show could not run. Its GeoJSON datasets took minutes to load
and then rendered at single-digit frame rates, because GeoJSON rendering issued one draw
call per primitive per feature and sampled terrain heights from the globe per draw
(the same underlying problem as upstream issue
[#3701](https://github.com/OpenSpace/OpenSpace/issues/3701)).

Nothing here changes the GeoJSON asset format or any documented property. All of it is
internal to how features are stored, culled and submitted.

## Contents

- [Measurement setup](#measurement-setup)
- [Results summary](#results-summary)
- [1. GeoJSON draw submission](#1-geojson-draw-submission)
- [2. GeoJSON per-frame CPU cost](#2-geojson-per-frame-cpu-cost)
- [3. GeoJSON visibility culling](#3-geojson-visibility-culling)
- [4. GeoJSON loading](#4-geojson-loading)
- [5. Terrain height sampling](#5-terrain-height-sampling)
- [6. Memory at very large feature counts](#6-memory-at-very-large-feature-counts)
- [7. Core engine: property owner registration](#7-core-engine-property-owner-registration)
- [8. Regional video overlays: compressed texture streaming](#8-regional-video-overlays-compressed-texture-streaming)
- [9. Build and toolchain](#9-build-and-toolchain)
- [Known conservatism and limitations](#known-conservatism-and-limitations)

## Measurement setup

Three datasets recur below:

| Name | Size | Contents |
| ---- | ---- | -------- |
| **Transmission lines** | 71 MB | 44,560 features, mostly `MultiLineString` |
| **TZ-SAM** | 300 MB | 322,630 features, `MultiPolygon` |
| **Toronto** | small | the existing `data/assets/examples` GeoJSON, used as a no-regression check |

Frame timings come from rolling perf counters added to `GeoJsonManager`, logged at
`LDEBUG` every 10 s (frame interval, emit ms, draw-submit ms, update ms, sub-draw counts
before and after merge, emit passes, cull counts). Headless runs need `--bypassLauncher`;
watch for the `GeoJson perf` line. Test profiles live under `user/data/profiles/`
(`geojson_tzsam_test`, `geojson_batch_test`, `geojson_dep_test`, `regionalvideo_test`).

Load timings distinguish **cold** (first load of a file) from **warm** (subsequent loads,
served from the on-disk cache).

## Results summary

| Metric | Dataset | Before | After |
| ------ | ------- | ------ | ----- |
| Cold load | Transmission lines | minutes | 3.2 s |
| Warm load | Transmission lines | — | 1.1 s |
| Cold load | TZ-SAM | not viable | 20 s |
| Warm load | TZ-SAM | — | 8.7 s |
| Frame time | Transmission lines | 250 ms (4 fps) | 3.8 ms |
| Frame time | TZ-SAM | not viable | 28.5 ms |
| Draw submit, regional view | TZ-SAM | 0.54 ms | 0.07 ms |
| `glMultiDrawArrays` sub-draws | TZ-SAM | 322,630 | 1 |
| `glMultiDrawArrays` sub-draws | Toronto | 140 | 1 |
| Per-feature CPU work, idle frame | any | O(n) | none |
| Globe height queries at load | TZ-SAM | ~21M | 0 |
| Height queries in a single frame | TZ-SAM | ~1.9M | budgeted, ≤30k vertices |
| `Property` objects | TZ-SAM | ~1.9M (~0.5 GB) | not created |

## 1. GeoJSON draw submission

**Commits** `2a835432a2` (points, lines), `27f8d7d0fb` (polygons)
**Files** `src/rendering/multidrawbatch.{h,cpp}` (new),
`modules/globebrowsing/src/geojson/geojsonmanager.{h,cpp}`,
`modules/globebrowsing/src/geojson/globegeometryfeature.{h,cpp}`,
`modules/globebrowsing/shaders/geojson_*.glsl`

The original path bound a program, set 11 uniforms, bound a VAO and issued a draw **per
feature per frame**, and allocated 3 GL objects per feature. At 322k features that is the
entire frame budget spent on driver overhead.

`rendering::MultiDrawBatch` is a new, renderable-agnostic facility that merges many small
draws sharing a program and vertex layout into `glMultiDrawArrays` calls:

- Retained CPU-side vertex streams per draw — `addDraw` / `removeDraw` / `commit`.
- A per-frame emit protocol — `beginFrame` → `emitDraw(handle, record, groupKey)` →
  `endFrame` — and one `glMultiDrawArrays` per group via `renderGroup`.
- `endFrame` sorts pending draws by group key, coalesces adjacent draws, and uploads a
  32-byte-per-draw SSBO (`GeoJsonDrawRecord`), which the shaders index by
  `baseDrawId + gl_DrawID`.

`GeoJsonManager` owns three batches — points, lines, polygons — shared by every GeoJSON
component on a globe. `GlobeGeometryFeature` registers vertex ranges instead of owning GL
objects, and emits one record per draw, so visibility, fades and live property edits need
no invalidation of GPU state.

Supporting changes that made batching possible:

- **Lines** are expanded to screen-space quads in a geometry shader, with width read from
  the per-draw record. This replaces `glLineWidth` — and removes its ~10 px driver cap —
  along with `GL_LINE_SMOOTH`.
- **Polygons** use a position-only 12-byte vertex stream. Every polygon triangle was
  already flat shaded (the non-tessellated, tessellated and extrusion paths each baked one
  normal per triangle), so the normal attribute is replaced by a screen-space-derivative
  normal computed in the fragment shader. Per-feature state moved into the shared SSBO
  record via a new `FlagPerformShading` bit; per-component lighting and the two-pass
  transparent-extrude cull state are encoded in the draw group keys, which preserves the
  previous component-major draw order.
- **Extrusion walls** (2 triangles per boundary edge — on large files, larger than the
  fill itself) are now built only when extrude is actually enabled, with a rebuild on
  toggle.

**Draw coalescing.** Adjacent draws in the same group with byte-identical records and
contiguous vertex ranges merge into a single sub-draw. A default-styled file therefore
collapses to one sub-draw per group: 322,630 → 1 on TZ-SAM, 140 → 1 on the Toronto
example. Enabled for points and triangles; line strips would join end-to-end, so lines
stay per-draw.

`addDraw` also had an O(n²) freed-slot scan that made the first geometry build hang at
hundreds of thousands of draws; it is now O(1) amortized.

## 2. GeoJSON per-frame CPU cost

**Commits** `ef8446cba7`, `27f8d7d0fb`
**Files** `geojsoncomponent.cpp`, `geojsonmanager.cpp`, `globegeometryfeature.cpp`,
`multidrawbatch.cpp`

The transmission-lines layer rendered at 4 fps even after batching, because several
per-frame costs still scaled with feature count. The governing invariant introduced here
is: **an idle frame does no per-feature work at all.** Everything in this section follows
from it.

- `GlobeGeometryFeature::render` rejects before any matrix math or GL work when no polygon
  render feature would draw. Previously every feature paid a `dmat4` inverse plus a VAO
  unbind and a GL state-cache reset per pass, even when it only had batched lines and
  undrawn extrusion polygons.
- `GeoJsonComponent::render` skips the entire polygon pass when the component provably has
  nothing to draw in it, tracked with counts of fill-polygon features and static extrude
  overrides plus one live property read.
- **Draw emission is change-driven.** The emitted draw lists and per-draw SSBO records are
  camera independent, so `GeoJsonManager` reuses them until a style-dirty flag fires. The
  flag is hooked to every property that affects the draws: component fades and scales,
  default style properties, per-subfeature enabled/opacity/fade, texture reloads and
  geometry recommits.
- `MultiDrawBatch::endFrame` skips the stable sort and record reshuffle when draws were
  emitted already grouped — the common single-group case.
- `GeoJsonComponent::isReady` caches its all-features scan once true.
- `GeoJsonComponent::update()` skips its whole feature loop on idle frames, gated on dirty
  flags, point-texture counts and resolved `RelativeToGround` usage. A never-cleared
  `_heightOffsetIsDirty` flag is now reset, so a single height-offset edit no longer makes
  every subsequent frame walk all features forever.

Result on the transmission-lines file: **250 ms → 3.8 ms** per frame.

## 3. GeoJSON visibility culling

**Commit** `a5a4d23d87`
**Files** `modules/globebrowsing/src/geojson/geojsonculling.{h,cpp}` (new), plus
manager / component / feature

Features whose bounding spheres fall outside a guard-banded view frustum, or that are
entirely occluded by the globe, are skipped when the batched draw lists are emitted.

- **Bounding spheres** are accumulated in model space from the actual built vertices
  during geometry rebuilds. They include extrusion walls, and need no cache format change.
- **Frustum test**: sphere against 5 planes derived from the projection tangents, widened
  by 15% as a guard band. The far plane is skipped because the polygon vertex shader
  clobbers `gl_Position.z`.
- **Horizon test**: shrunken-occluder tangent-sum against `ellipsoid.minimumRadius()`.
- **Re-emit policy**: cull results carry slack — a translation threshold, height padding
  and point-billboard slack — which keeps them valid anywhere inside a ball of camera
  motion. An O(1) per-frame check re-emits only when the camera leaves that ball, so the
  zero-idle-cost invariant from §2 survives. The height refinement sweep (§5) forces a
  re-emit when grown samples exceed the slack of the last cull.
- Frames with several render passes (multi-viewport, stereo) share one set of draw lists
  across differing frusta, so frustum culling is suppressed for them; horizon culling
  stays on.
- Exposed as `PerformFrustumCulling` and `PerformHorizonCulling` on the globe's
  `GeographicOverlays` owner, both default true, `AdvancedUser` visibility.

At the TZ-SAM regional test view, 99% of features are culled and draw submission drops
from **0.54 ms → 0.07 ms**.

A note for anyone extending this: the scene updates **twice per frame**
(`preSynchronization` and `postSynchronizationPreDraw` both call
`RenderEngine::updateScene()`), so any update-side counter of render passes must only
latch when render calls actually happened in between.

## 4. GeoJSON loading

**Commit** `2a835432a2`
**Files** `geojsonparser.{h,cpp}` (new), `geojsoncache.{h,cpp}` (new),
`geojsoncomponent.cpp`, `modules/globebrowsing/ext/glaze` (new submodule)

- **Parsing**: GeoJSON is read with glaze instead of the geos nlohmann-DOM reader, with
  coordinates read directly into their final types. This also preserves z values for
  `MultiLineString` coordinates, which the geos reader silently dropped.
- **Derived-data cache**: the fully derived per-feature data — geodetic coordinates,
  triangulated polygons, style overrides, GUI metadata — is cached as BEVE through ghoul's
  `CacheManager`, keyed on a cache version constant, the ignore-heights flag and the source
  file's modification time. A warm load skips JSON parsing, GEOS, `MakeValid`,
  triangulation and centroid/envelope work entirely. Bumping `CacheVersion` invalidates
  every cache.
- **`MakeValid` on demand**: run only on features whose geometry is actually invalid,
  rather than on all of them.
- A per-file load summary with phase timings is logged, which is what made the rest of this
  measurable.

Transmission lines: **minutes → 3.2 s cold, 1.1 s warm.** TZ-SAM: **20 s cold, 8.7 s
warm.**

## 5. Terrain height sampling

**Commit** `74d484568b`
**Files** `globegeometryfeature.{h,cpp}`, `geojsoncomponent.{h,cpp}`, `geojsoncache.{h,cpp}`

In `RelativeToGround` mode the original code sampled a height-map value per vertex at load
time. Globe height queries return 0 until the relevant tiles have streamed in, so this
produced roughly **21M useless chunk-tree queries** per large file — a multi-second stall —
and mostly-zero results regardless.

- Geometry now builds with **zero-filled heights**; no globe queries at load.
- A **budgeted refinement sweep** raises them as terrain streams in: a component-level
  round-robin over features with per-frame budgets of 1.5 ms of reference-point change
  checks and 30k re-sampled vertices, completing a full pass at most every 10 s. This
  replaces per-feature 10 s timers that all expired in the same frame — about **1.9M globe
  queries in one frame** at 322k features. The sweep splits into `checkHeightMapChange` and
  `applyHeightUpdate`.
- Fixes a latent bug where a re-sample never refreshed the feature's control heights, so
  any feature that changed once re-sampled all of its vertices every 10 s forever.
- Globes with no active height layers skip the sweep entirely, and the force-update trigger
  restarts the sweep in forced mode instead of re-sampling everything in one frame.
- **Heights sidecar cache**: refined heights persist across runs in a BEVE sidecar next to
  the geometry cache (`GeoJsonHeightsCacheFile`), written on shutdown or layer deletion
  when any feature was re-sampled. It stores both the sampled heights and the control
  heights they were sampled against, and is keyed on the globe identifier, the lat/long
  offset and the same source modification time as the geometry cache. Cached vectors
  install positionally into the deterministic render-feature build order and are validated
  by vertex count; any mismatch — changed tessellation or extrude configuration — falls
  back to fresh refinement, so configuration drift self-heals.

TZ-SAM with the SRTM tileset: sweep costs 1.8–3.2 ms per frame at a steady 31 ms frame
time, the 100 MB sidecar round-trips, and a warm start begins at the refined heights with
the first sweep a no-op. `RelativeToGround` remains opt-in per layer or per feature.

Height-map sampling is now skipped entirely unless a feature actually resolves to
`RelativeToGround`; changing the altitude mode rebuilds with real heights.

## 6. Memory at very large feature counts

**Commit** `27f8d7d0fb`

Above a configurable feature count — `PerFeaturePropertiesThreshold`, default 10,000 — the
per-feature property owners (enable / fade / fly-to) are no longer created. A plain meta
vector serves fly-to and bounding-box computation instead.

At 322k features those owners would cost roughly **1.9M `Property` objects and ~0.5 GB of
RAM**, and would make a WebGUI connect response serialize all of them.

## 7. Core engine: property owner registration

**Commit** `2a835432a2`
**File** `src/properties/propertyowner.cpp`

Two O(n²) behaviours surfaced with tens of thousands of sub-owners. Both are core-engine
changes that benefit any subsystem registering many property owners, not just GeoJSON:

- `propertySubOwner` and `addPropertySubOwner` did a linear `std::find_if` over
  `_subOwners`. A `_subOwnerIndex` hash map keyed by identifier makes lookup and
  duplicate-detection O(1). `setIdentifier` re-keys the parent's index, so renames stay
  consistent.
- `addPropertySubOwner` called `updateUriCaches()` on the whole tree. Only the newly added
  owner's subtree gets new URIs, so it now calls `owner->updateUriCaches()`.

`removePropertySubOwner` additionally tolerates the render engine having no active scene
(shutdown, unit tests), where previously it dereferenced a null scene.

## 8. Regional video overlays: compressed texture streaming

**Commits** `f9b6f8cbb7`, `05622827a1`
**Files** `modules/video/{include,src}/{videotiming,texturevideostream,asyncvideoframedecoder,texturevideoplayer,regionalvideotileprovider}.*`

`RegionalVideoTileProvider` renders a video over a lat/lon extent of any `RenderableGlobe`.
It is a new capability rather than a speed-up of an existing one, but its runtime was
designed around the same constraints, and it is deliberately cheaper than the existing
libmpv-backed `VideoTileProvider` (which is untouched and still available):

- **No runtime video codec.** The source is a preprocessed Basis Universal ETC1S KTX2
  "texture video" — frames as array layers, conditional replenishment — transcoded on a
  worker thread to BC7 (BC3 fallback) and uploaded as compressed GPU textures. Disk cost is
  ~0.2–1.25 bits/texel and VRAM is **8:1 against RGBA**.
- **No `Syncable`.** Playback maps to simulation time as a pure function of the
  cluster-synchronized clock, so nothing has to be synchronized across nodes — unlike the
  mpv-based provider.
- **Asynchronous decode**: a single stateful worker thread (ETC1S conditional-replenishment
  decode is strictly sequential), direction-aware prefetch of 8 frames, a capacity-evicted
  32-frame cache, and a non-blocking `frameFor` so the render thread never waits.
  Out-of-order transcode *silently* returns wrong pixels — verified empirically — so
  `planDecode` explicitly chooses between continuing and restarting from a keyframe.
- **Bounded subdivision**: `maxLevel` is derived from video resolution against extent size,
  and `tileStatus` returns `OutOfRange` outside the extent so the layer never drives chunk
  subdivision there.

`05622827a1` fixes a UV transform bug under tile-tree ascension that caused ghost copies of
the video at 2×/4× scale and full repeats inside every chunk deeper than `maxLevel`. The
shader applies each pile entry's transform to the rendered chunk's own [0,1] UV, and the
frame texture is identical at every level, so the transform must be invariant under
ascension; the ascend path now mutates only the tile index.

Measured: overlay confined exactly to the CONUS extent over Blue Marble, ~60 FPS vsync-
locked and ~212 FPS uncapped, zero GL errors.

## 9. Build and toolchain

**Commits** `9198b2fda0`, `fdde6b2406`, `4169812841`, `0c63a59236`, `54412fe0d9`

Build-time rather than runtime, but part of the same effort:

- A `windows-llvm` CMake preset (clang-cl + lld-link, Ninja Multi-Config) alongside the
  Visual Studio `windows-min` preset, with the source fixups clang's stricter checks
  require pushed into the ghoul and sgct submodules rather than carried as local patches.
- `/force:multiple` applied centrally in `create_new_application` for Clang, because zlib
  is bundled both standalone and inside CDF; MSVC's linker resolves the duplicate via
  archive member selection, lld-link does not.
- `support/deploy.sh` — a bash packaging path that builds through the LLVM preset, stages
  non-destructively, and validates the resulting archive (manifest assertions plus a
  headless `--help` smoke test of the packaged binaries).

Several rendering fixes landed in the sgct fork along the way and are worth upstreaming:
the `renderViewports` final-blit direction, the no-postfx fallback path, an FXAA shader
interface-block name mismatch, and an MSAA early-return that made any `msaa > 1`
configuration render nothing.

## Known conservatism and limitations

Stated explicitly so reviewers do not have to find them:

- **Horizon culling is conservative near the equator.** It tests against
  `ellipsoid.minimumRadius()`, so oblateness means equatorial features cull less than they
  could. This is the same conservatism the existing chunk culling already has.
- **Frustum culling is disabled on multi-pass frames** (multi-viewport, stereo) because the
  draw lists are shared across passes with different frusta. Horizon culling still applies.
- **Line draws do not coalesce.** Merging adjacent line strips would join them end-to-end,
  so only points and triangles coalesce.
- **The heights sidecar can be large** — 100 MB for TZ-SAM — and is written on shutdown or
  layer deletion.
- **`PerFeaturePropertiesThreshold` changes the property tree.** Above the threshold,
  per-feature enable/fade/fly-to properties do not exist. Anything scripted against them
  will not find them on very large files.
- **Antimeridian-crossing extents are rejected** by `RegionalVideoTileProvider`; a
  documented v1 limitation.
- **Not yet exercised** for the video path: multi-node cluster runs, RenderDoc upload
  inspection, and pixel-level registration against a graticule.

## Where the detail lives

The implementation record, including dead ends and the reasoning behind each choice, is in
the notes submodule: `notes/openspace/geojson-perf-internals.md` for the GeoJSON work and
`notes/openspace/changes.md` for the video overlays.
