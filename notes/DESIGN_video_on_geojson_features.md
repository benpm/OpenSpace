# Design: Projecting video onto GeoJSON features

Framework for rendering video clipped/projected onto **arbitrary geographic regions of a
planet defined by GeoJSON features** — the polygon generalization of
[DESIGN_regional_video_overlays.md](DESIGN_regional_video_overlays.md), which covers the
rectangular (lat/lon bbox) case. Read that doc first; this one builds on it and shares its
`VideoPlayer` + extent→UV foundation.

> Compiled 2026-07-02 from source on branch `project/geojson_perf`. All file:line refs
> verified against this tree.

---

## 1. Verified pipeline facts this design rests on

| Fact | Where |
|---|---|
| Tile layers sample via `uvOffset + uvScale * tileUV` per chunk tile | `texturetilemapping.glsl:116-118` (`tileUVToTextureSamplePosition`), struct at `tile.glsl:33-36` |
| **`Normal` layer blending is true alpha-over** — a texel with `a=0` leaves lower layers untouched | `blendNormal`, `modules/globebrowsing/shaders/blending.glsl:28-31`; alpha survives layer settings (`texturetilemapping.glsl:110-114`) |
| Layer blend modes: Normal / Multiply / MultiplyMix / Add / Subtract / Color | `layergroupid.h:180-201`, `texturetilemapping.glsl:176-207` |
| Chroma-key adjustment already exists per layer (keyed color → `vec4(0)`) | `texturetilemapping.glsl:221-238` |
| **No polygon/stencil/clip mask mechanism exists for layers** (WaterMask is a reflectance texture, not a clip) | grep sweep of `modules/globebrowsing` — only chroma key + water mask |
| `VideoPlayer` frame texture: RGBA8 FBO attachment mpv renders into; **default sampler = `GL_REPEAT` + `GL_LINEAR`** | `videoplayer.cpp:1006-1029` (`SamplerInit{}` → ghoul `texture.h:141-159` defaults). Any regional/clipped use must switch to `ClampToBorder` with border `(0,0,0,0)` |
| `VideoTileProvider::chunkTile` builds whole-globe UV from `(x, y, level)`; `ascendToParent` just decrements level | `videotileprovider.cpp:120-137` |
| `MapToSimulationTime`: j2000 time → clamped fraction of `[StartTime, EndTime]` → video time | `videoplayer.cpp:975-988` |
| GeoJSON polygons are CDT-triangulated meshes (holes supported) with **per-vertex `Geodetic2` retained CPU-side** for every post-tessellation vertex | `globegeometryfeature.cpp:165-289`, `globegeometryhelper.cpp:81-90` |
| GeoJSON polygon shader path has **no UVs and no texture sampling** (points only) | `geojson_fs.glsl:47-74`; VAO layout `globegeometryfeature.cpp:759-816` |
| GeoJSON renders in `renderSecondary`, i.e. **after** the tile-layer pass, plain `SRC_ALPHA` blending into the G-buffer | `renderableglobe.cpp:969-980`, `geojsoncomponent.cpp:521-522` |
| Tile pipeline color layers always carry alpha (BGRA8) | `tiletextureinitdata.cpp:112-147` |

Details on the GeoJSON side: [globebrowsing-geojson.md §10](globebrowsing-geojson.md).

---

## 2. The two architectures

There are exactly two places video can meet a polygon, and they serve different use cases:

```
A. Tile-layer route (compositing)          B. Geometry route (mesh texturing)
   video frame ─┐                             video frame ─┐
                ├→ masked frame texture                    ├→ sampled in geojson_fs
   polygon mask ┘   → RegionalVideoTile-      polygon mesh ┘   via new per-vertex UVs
                      Provider → layer
                      stack (per-tile UV,
                      LOD, blend modes)
```

| | **A: polygon-masked video layer** | **B: video-textured GeoJSON mesh** |
|---|---|---|
| Clipping fidelity | mask texture resolution | exact (mesh *is* the polygon, holes incl.) |
| Registration with imagery | pixel-exact (same tile pipeline) | ~1 m (float32 model-space verts) |
| Terrain conformance | per-pixel (tile heights) | per-vertex (tessellation density) |
| Blend modes / layer settings / chroma key | free (existing layer machinery) | fixed alpha-over in secondary pass |
| Per-feature UX (enable, fade, fly-to) | no (it's one layer) | free (`SubFeatureProps`) |
| Extrusion / floating above surface | no | free (`Extrude`, `HeightOffset`) |
| New machinery | mask bake + composite pass | UV VBO + shader + texture plumbing |

**Use A** when the video is a *data overlay* that must register with imagery (animated
energy/weather maps clipped to a country/state boundary). **Use B** when the *feature is
the presentation object* (highlight a polygon and play media inside it, possibly extruded
or floating). They share the `VideoPlayer` and the extent→UV math, so building one does
most of the other's groundwork.

---

## 3. Architecture A — polygon-masked regional video layer

Extends `RegionalVideoTileProvider` from the bbox design with one optional parameter:

```lua
Type = "RegionalVideoTileProvider",
Video = asset.resource("energy_us.mp4"),
Extent = { MinLon = -125.0, MinLat = 24.0, MaxLon = -66.5, MaxLat = 49.5 },
-- NEW: clip to features from a GeoJSON file (polygons only; union of all features)
ClipGeoJson = asset.resource("us_boundary.geojson"),
ClipFeather = 0.0,  -- optional edge softening in mask texels
```

### 3.1 Mask baking (once, at load)
- Parse `ClipGeoJson` with GEOS exactly as `GeoJsonComponent::readFile` does (GEOS is
  already a globebrowsing dependency; the provider lives in `modules/video`, which already
  links globebrowsing).
- Triangulate polygons with `ConstrainedDelaunayTriangulator` (same call as
  `globegeometryfeature.cpp:190-209` — factor the triangulation into a shared helper in
  `globegeometryhelper.h` rather than duplicating).
- Rasterize the triangles **in lon/lat space normalized to the Extent bbox** into a
  single-channel `R8` FBO (e.g. 2048×2048, configurable). This is a trivial ortho draw of
  the triangle soup — no CPU point-in-polygon needed, holes come out correct from CDT.
- Optional `ClipFeather`: separable box blur on the mask, or sample the mask with linear
  filtering and a smoothstep in the composite shader. v1: linear filtering only.
- If `Extent` is omitted, derive it from the GeoJSON envelope (GEOS `getEnvelope`).

### 3.2 Per-frame composite
When (and only when) `VideoPlayer` produces a new frame, run one small fullscreen pass
into a second RGBA8 FBO texture:

```glsl
out.rgb = texture(frame, uv).rgb;
out.a   = texture(frame, uv).a * texture(mask, uv).r;
```

The provider hands *this* texture to the tile pipeline instead of the raw frame. Because
`blendNormal` honors alpha (§1), masked-out texels show the layers below — no layer-shader
changes at all. Set the composite texture (not just the frame texture) to
`ClampToBorder(0,0,0,0)` so straddling/low-LOD tiles sample transparent outside the extent
(the raw frame's `GL_REPEAT` default would tile the video across the globe — this is a bug
waiting to happen in any naive implementation, see §1).

Cost: one 2 K×2 K pass per *video* frame (not per render frame), plus the mask bake once.

### 3.3 Degenerate case = the bbox design
`ClipGeoJson` absent → mask is all-ones → identical to the plain regional provider. So A
is strictly an increment on the prior design, not a fork. Ship order: bbox provider first,
mask second.

---

## 4. Architecture B — video-textured GeoJSON polygons

### 4.1 UV plumbing in globebrowsing (works for any texture, not just video)
The enabler is that `initializeRenderFeature` already computes
`feature.vertices : vector<Geodetic2>` for every final vertex
(`globegeometryfeature.cpp:763`). Add:

1. **VBO 2 `in_uv` (vec2)** — computed right next to `feature.vertices`:
   `uv = ((lon - minLon)/spanLon, (lat - minLat)/spanLat)` against a *texture extent*
   that defaults to the feature envelope (already computed for
   `SubFeatureProps::boundingboxLatLong`) and can be overridden per component
   (`TextureExtent = {...}`) so one image/video can span multiple features consistently.
2. **`geojson_vs.glsl`**: pass `in_uv` through the `Data` block.
3. **`geojson_fs.glsl`**: `uniform sampler2D fillTexture; uniform bool useFillTexture;`
   → `frag.color = useFillTexture ? texture(fillTexture, uv) * vec4(1,1,1, fillOpacity)
   : vec4(color, fillOpacity)`. Lines/points unaffected.
4. **`GeoJsonProperties`**: add `FillTexture` (string path) + optional `TextureExtent`,
   mirroring how `PointTexture` already flows through
   `GeoJsonProperties`/`GeoJsonOverrideProperties`/`PropertySet`
   (geojsonproperties.h:89,111,139) — so it works from asset `DefaultProperties`, from
   per-feature GeoJSON `properties`, and live in the UI. Reuse `TextureComponent` for
   loading (the points path already does, `globegeometryfeature.cpp:146-152`).

This step alone (static images on polygons) is independently useful and fully contained
in globebrowsing. Estimated ~1 day.

### 4.2 Wiring video in (dependency direction matters)
`modules/video` depends on globebrowsing (it registers `VideoTileProvider` with
globebrowsing's TileProvider factory), **not** vice versa — so `GeoJsonComponent` must not
know about `VideoPlayer`. Options:

- **(a) Texture-injection hook (recommended).** Globebrowsing exposes on
  `GeoJsonComponent` (or `GeoJsonManager`) a
  `setDynamicFillTexture(featureKey, ghoul::opengl::Texture*)` API. A small new
  scene-graph-attachable class in `modules/video` (e.g. `GeoJsonVideoTexture`: owns a
  `VideoPlayer`, resolves globe + component + feature by identifier, pushes
  `frameTexture()` every update). Playback, sim-time mapping, and cluster sync stay
  entirely in `VideoPlayer`.
- **(b) Subclass in video module.** A `VideoGeoJsonComponent` living in modules/video.
  Heavier: component construction is funneled through `GeoJsonManager`/Lua
  (`addGeoJson`), which would need a factory seam it doesn't have today.
- **(c) Screen-space/RenderablePlane hacks.** Rejected — same reasons as the flat-plane
  alternative in the bbox doc (§8 there).

v1 = 4.1 with static textures; v2 = (a).

### 4.3 Caveats to document up front
- Mesh vertex positions are float32 model-space → ~0.5–1 m quantization on Earth; don't
  promise sub-meter registration against imagery.
- Terrain fit is per-vertex: a coarsely tessellated polygon over rough terrain will let
  the video "float" between vertices where a tile layer would hug per-pixel. Mitigate
  with `Tessellation.TessellationDistance`.
- The two-pass extrude path multiplies fill draws (`globegeometryfeature.cpp:317-320`);
  video-textured + extruded + translucent is the worst case — fine, but note it.
- Antimeridian-crossing features: same v1 restriction as the bbox design (reject or
  split); UVs would wrap otherwise.

---

## 5. Shared foundation ("the framework")

To keep A and B from diverging, factor these once:

1. **`GeoExtent` helper** (lon/lat rect, radians): construction from Lua table or GEOS
   envelope, `contains/overlaps` (the bbox doc's `GeodeticPatch::overlaps` does the
   tile-side test), and the extent→UV mapping used identically by the tile provider
   (per-tile `TileUvTransform`), the mask baker (ortho projection), and the mesh UV
   generator (per-vertex). One implementation, three call sites, one unit test.
2. **Shared triangulation helper** in `globegeometryhelper` (GEOS polygon → triangle
   soup) used by both `GlobeGeometryFeature` and the mask baker.
3. **`VideoPlayer` as-is** for decode/playback/sync — both routes consume only
   `frameTexture()` + `update()`. The single required change: expose/set sampler wrapping
   (`ClampToBorder`, transparent border) — today it's ghoul's `Repeat` default
   (`videoplayer.cpp:1029`, ghoul `texture.h:147`).

### Build order
1. `RegionalVideoTileProvider` (bbox) — prior doc, unlocks the Digital Earth use case.
2. Mask bake + composite (§3) — polygon clipping for overlays.
3. GeoJSON UV + `FillTexture` (§4.1) — static images on features.
4. `GeoJsonVideoTexture` injection (§4.2a) — video on features.

Each step ships independently useful capability; none blocks the previous.

### Interim zero-code trick
For video whose subject sits on a solid background color, the existing per-layer
**chroma key** (`texturetilemapping.glsl:221-238`) can already knock the background out to
transparency on a whole-globe or (once built) bbox video layer — a usable stopgap for
irregular shapes before the mask lands.

---

## 6. Validation additions (beyond the bbox doc's §6)

- **Mask correctness**: polygon with a hole → hole shows lower layers; feature exactly on
  the extent edge → no bleed (border sampling), no repeat-tiling anywhere on the globe.
- **UV continuity (B)**: adjacent tessellated triangles sample continuously (UVs are
  linear in lon/lat so this is exact, but verify after `subdivideTriangle` since new
  verts get UVs from *recovered* geodetic coords — float roundtrip through
  `cartesianToGeodetic2`).
- **A/B cross-check**: same polygon + same video via both routes → visually coincident
  within mesh precision.
- **New-frame-only compositing**: confirm the composite pass runs per video frame, not
  per render frame (tie to the same `frameTexture` pointer-change signal
  `VideoTileProvider::tile` already uses, `videotileprovider.cpp:83-92`).

## 7. File touch-list (delta over the bbox design)

- **A**: `modules/video/src/regionalvideotileprovider.*` (+`ClipGeoJson`/mask/composite),
  small composite shader in `modules/video/shaders/`;
  `modules/globebrowsing/src/geojson/globegeometryhelper.*` (factored triangulation).
- **B**: `modules/globebrowsing/src/geojson/{geojsonproperties.*, globegeometryfeature.*}`
  (UV VBO, `FillTexture`), `modules/globebrowsing/shaders/geojson_{vs,fs}.glsl`;
  later `modules/video/src/geojsonvideotexture.*` + registration in `videomodule.cpp`.
- **Shared**: `GeoExtent` (globebrowsing `src/basictypes.h` or a new small header) +
  tests; `VideoPlayer` sampler-wrapping setter (`modules/video/src/videoplayer.*`).
