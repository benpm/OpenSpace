# Design: Geospatially-bounded video & animated raster overlays on globe surfaces

Implementation design for TODO item 3 / draft issue #3
("Support geospatially-bounded video and animated raster overlays on globe surfaces").

Goal: place a video — or an animated raster sequence — over **only a geographic
sub-region** of a globe (a lat/lon bounding box), composited over the existing imagery,
with playback tied to application or simulation time, working on any planet and in
clustered multi-window setups.

---

## 1. What already exists (and why the gap is narrow)

| Capability | Mechanism today | Regional? |
|---|---|---|
| Static regional imagery | `DefaultTileProvider` → GDAL. The geotransform maps each tile's `GeodeticPatch` to a pixel window; tiles outside the dataset extent resolve to no data. | **Yes, already works.** A GeoTIFF covering only the US renders only over the US. |
| Animated raster sequence | `ImageSequenceTileProvider` (frame index) / `TemporalTileProvider` (time-keyed), each frame a `DefaultTileProvider` over a georeferenced raster. | **Mostly works** — each frame is geo-referenced, so regional extent is inherited from the source rasters. Gaps are ergonomics, performance, and simulation-time wiring, not capability. |
| Video on a globe | `VideoTileProvider` (modules/video) wraps one `VideoPlayer` (libmpv → GL texture) and returns that **same frame texture for every tile**, mapped equirectangularly across the **whole** globe. | **No.** Video has no geographic awareness; it always covers the entire sphere. |

Key takeaways:
- `modules/globebrowsing/src/tileprovider/videotileprovider.*` lives in the **video**
  module but subclasses globebrowsing's `TileProvider` and is registered with the
  `TileProvider` factory (`videomodule.cpp:46-50`). A new regional variant follows the
  same pattern.
- The tile pipeline already has the exact primitive we need to crop a texture to a
  sub-tile region: **`TileUvTransform { uvOffset, uvScale }`** (basictypes.h:132). The
  shader samples `textureUV = uvOffset + uvScale * tileUV` (texturetilemapping.glsl).
- `VideoTileProvider::chunkTile` (videotileprovider.cpp:120-137) already builds a
  per-tile `TileUvTransform` from the tile's `(x, y, level)`. The regional version
  differs only in *what rectangle* it maps the video onto: the **extent**, not the whole
  globe.

So the feature is two narrowly-scoped pieces:
1. **Regional video** → new `TileProvider` that maps the video frame onto a lat/lon
   extent instead of the whole globe (the bulk of this doc, §2–§5).
2. **Regional animated raster** → thin authoring/sync improvements over the existing
   sequence/temporal providers (§7).

---

## 2. Core idea: extent → per-tile UV transform + transparent border

Treat the video frame as a plate-carrée image covering exactly the extent rectangle
`E = [minLon, maxLon] × [minLat, maxLat]` (radians). For a tile whose `GeodeticPatch`
spans `[tLonMin, tLonMax] × [tLatMin, tLatMax]`, the linear map tile-UV → video-UV is:

```
uvScale.x  = (tLonMax - tLonMin) / (E.maxLon - E.minLon)
uvOffset.x = (tLonMin - E.minLon) / (E.maxLon - E.minLon)
// y is flipped (texture v grows south→north opposite to the globe's tile-y), mirroring
// the existing VideoTileProvider y handling:
uvScale.y  = (tLatMax - tLatMin) / (E.maxLat - E.minLat)
uvOffset.y = (tLatMin - E.minLat) / (E.maxLat - E.minLat)
```

Three tile cases fall out automatically from this single formula:

- **Fully inside E** → `uvOffset, uvOffset+uvScale ∈ [0,1]`: samples the correct sub-rect
  of the frame.
- **Fully outside E** → return an empty tile (`Tile::Status::OutOfRange`); the renderer's
  `traverseTree` (tileprovider.cpp:143-206) leaves the chunk to lower layers. Cheap and
  exact at the tile granularity.
- **Straddling E's boundary** (or a low-LOD tile larger than E, giving `uvScale > 1` and
  negative offsets) → the in-extent part maps into `[0,1]`, the out-of-extent part maps
  **outside** `[0,1]`. Set the frame texture's wrap mode to **`GL_CLAMP_TO_BORDER`** with
  border color `(0,0,0,0)`, so everything outside the extent samples transparent and
  blends through to the layers below. **No shader change required.**

This is the crux: the whole crop is expressed through data the pipeline already plumbs
(`TileUvTransform` + texture sampler state). Validation point in §6.

---

## 3. New class: `RegionalVideoTileProvider`

New files `modules/video/include/regionalvideotileprovider.h` /
`modules/video/src/regionalvideotileprovider.cpp`, registered in `videomodule.cpp`
alongside `VideoTileProvider`:

```cpp
fTileProvider->registerClass<RegionalVideoTileProvider>("RegionalVideoTileProvider");
```

Shape mirrors `VideoTileProvider` (owns a `VideoPlayer`, forwards `update`/`reset`,
`addPropertySubOwner(_videoPlayer)`), with an extent and a tighter `maxLevel`:

```cpp
class RegionalVideoTileProvider : public TileProvider {
public:
    explicit RegionalVideoTileProvider(const ghoul::Dictionary& dictionary);

    void update() override final;          // _videoPlayer.update()
    void reset() override final;           // _videoPlayer.reload()
    int minLevel() override final;         // 1
    int maxLevel() override final;         // see §5 (derive from extent, not 1337)
    ChunkTile chunkTile(TileIndex, int parents, int maxParents = 1337) override;
    Tile tile(const TileIndex&) override final;
    Tile::Status tileStatus(const TileIndex&) override final;
    TileDepthTransform depthTransform() override final;   // { 0.f, 1.f }
    static Documentation Documentation();

private:
    void internalInitialize() override final;   // _videoPlayer.initialize() + set border wrap
    void internalDeinitialize() override final; // _videoPlayer.destroy()

    GeodeticPatch _extent;     // built from the Extent parameter
    VideoPlayer _videoPlayer;
    std::map<TileIndex::TileHashKey, Tile> _tileCache;
};
```

### `tile()` — extent-cull then return the shared frame
```cpp
Tile RegionalVideoTileProvider::tile(const TileIndex& tileIndex) {
    if (!_videoPlayer.isInitialized()) return Tile();

    const GeodeticPatch tilePatch(tileIndex);
    if (!tilePatch.overlaps(_extent)) {           // add overlaps() to GeodeticPatch (§4)
        return Tile{ nullptr, std::nullopt, Tile::Status::OutOfRange };
    }
    // Same single-frame caching pattern as VideoTileProvider, keyed by hash, invalidated
    // when the frame texture pointer changes.
    return Tile{ _videoPlayer.frameTexture().get(), std::nullopt, Tile::Status::OK };
}
```

### `chunkTile()` — UV transform from extent
Replace VideoTileProvider's whole-globe offset/ratio math with the §2 formula computed
from `GeodeticPatch(tileIndex)` and `_extent`. Reuse the base `traverseTree` /
`ascendToParent` plumbing (the `ascendToParent` lambda just decrements level; the parent's
UV is recomputed from its own patch on the next call, so no incremental UV bookkeeping is
needed — recompute from the patch each time for clarity).

### `tileStatus()`
`OutOfRange` above `maxLevel()` **or** when the tile patch doesn't overlap `_extent`;
otherwise `OK` once the player has a frame. Returning `OutOfRange` for non-overlapping
tiles is what keeps the overlay confined and avoids paying for off-region chunks.

---

## 4. Small globebrowsing helper: `GeodeticPatch::overlaps`

`GeodeticPatch` (geodeticpatch.h) already exposes `minLat/maxLat/minLon/maxLon` and a
`TileIndex` constructor. Add an axis-aligned overlap test (with longitude-wrap care):

```cpp
bool GeodeticPatch::overlaps(const GeodeticPatch& o) const {
    return maxLat() >= o.minLat() && minLat() <= o.maxLat() &&
           maxLon() >= o.minLon() && minLon() <= o.maxLon();
}
```

Antimeridian-crossing extents (minLon > maxLon) are an edge case; v1 can document
"extent must not cross ±180°" and reject it at load, or split into two providers. Note it,
don't over-build it.

---

## 5. Asset / parameter surface

Reuse the whole `VideoPlayer` parameter block (Video, PlaybackMode, StartTime, EndTime,
LoopVideo, PlayAudio, PlayDelay — videoplayer.cpp:134-162) via the same
`codegen::doc<Parameters>(..., VideoPlayer::Documentation())` composition VideoTileProvider
uses. Add one new required field:

```lua
ColorLayers = {
  {
    Identifier = "US_Energy_Animation",
    Type = "RegionalVideoTileProvider",
    Video = asset.resource("energy_us.mp4"),
    -- degrees; converted to radians into _extent
    Extent = { MinLon = -125.0, MinLat = 24.0, MaxLon = -66.5, MaxLat = 49.5 },
    PlaybackMode = "MapToSimulationTime",
    StartTime = "2020 01 01 00:00:00",
    EndTime   = "2020 01 02 00:00:00",
    LoopVideo = false,
    Opacity = 0.85,
    BlendMode = "Normal"
  }
}
```

- **`Extent`** is the only new parameter. Optional future ergonomics: read it from a
  sidecar world/`.prj` file, or accept GDAL corner syntax, but explicit lat/lon is the
  v1 contract (most video carries no geo metadata).
- **`maxLevel()`**: don't hardcode 1337. Derive a sane cap from extent size and video
  resolution (pixels-per-degree → highest level where the video still adds detail) so a
  small overlay doesn't force deep subdivision of a tiny patch. Conservative v1: a fixed
  cap (e.g. 19) is fine; document the heuristic as a follow-up.
- **Multiple overlays**: each is just another `Layer` in `ColorLayers` (or `Overlays`).
  Stacking, opacity, z-index, and blend modes are inherited from the existing layer
  system — no new machinery for "multiple simultaneous overlays."
- **Planet-agnostic**: nothing here is Earth-specific; the provider only sees
  `GeodeticPatch`. Works on any `RenderableGlobe`.

---

## 6. Validation plan

The design leans on two behaviors that must be confirmed early, plus standard checks:

1. **Transparent-border sampling (the load-bearing assumption).** Confirm that setting the
   `VideoPlayer` frame texture to `GL_CLAMP_TO_BORDER` with border `(0,0,0,0)` makes
   out-of-extent samples transparent **and** that the color-layer `Normal` blend honors
   src alpha (texturetilemapping.glsl). If blending ignores alpha, fall back to a
   **shader clip**: pass the layer extent + interpolate per-fragment geodetic coords and
   `discard`/zero-alpha outside `[0,1]` UV. Prototype this first on a static
   `SingleImageProvider`-style test before wiring video.
2. **UV math** — unit-test the extent→`TileUvTransform` mapping: a tile exactly equal to
   the extent yields `uvOffset=(0,0)`, `uvScale=(1,1)` (with correct y-flip); a tile at
   the NW corner of E maps to the NW corner of the frame. Pure function, no GL needed —
   testable in `OpenSpaceTest` like the existing globebrowsing math tests.
3. **Extent culling** — tiles outside E return `OutOfRange`; assert lower layers remain
   visible there (no black hole, no smear from `CLAMP_TO_EDGE`).
4. **Alignment** — overlay a regional video whose content matches a known static GeoTIFF
   layer of the same extent; confirm pixel-level registration against the globe graticule.
5. **Time + cluster sync** — `MapToSimulationTime` advances with OpenSpace `Time`;
   `VideoPlayer`'s existing master/slave `encode/decode` path (videoplayer.cpp:864-951)
   already covers multi-window sync — verify it still holds for the regional provider
   (it should: the provider doesn't touch playback, only tile mapping).

---

## 7. Animated raster sequences (the cheaper half)

For **animated maps that are raster sequences** rather than mp4, prefer the existing
geo-aware providers — each frame is already a georeferenced `DefaultTileProvider`, so
regional extent and correct projection come for free:

- `ImageSequenceTileProvider` — frame-index driven.
- `TemporalTileProvider` — time-keyed (folder-of-rasters or WMS time prototype), which is
  the natural fit for simulation-time-driven energy/weather animations.

The realistic gaps here are **not** rendering capability but:
- **Authoring**: a profile/asset helper to declare "a folder of timestamped regional
  GeoTIFFs as one temporal layer" (overlaps with #2830, #4025).
- **Performance**: prefetch/decode-ahead and eviction so frame stepping doesn't stall
  (relates to the GDAL "layers stop rendering after a while" bug, draft issue #1).
- **Sim-time sync**: ensure `TemporalTileProvider` frame selection and `VideoPlayer`'s
  `MapToSimulationTime` use a consistent time source so a video overlay and a temporal
  raster overlay stay in lockstep.

Recommendation: ship §2–§6 (regional **video**) as the new capability, and treat regional
**animated rasters** as an authoring + perf track on top of the existing temporal
providers rather than a new renderer.

---

## 8. Alternatives considered (and why not)

- **Decal / projected-texture onto terrain** (project the video from above using a
  frustum, like shadow mapping). More general (arbitrary orientation) but a much larger
  renderer change, new shader pass, and depth/precision headaches. Overkill for
  axis-aligned lat/lon overlays.
- **`RenderableVideoPlane` positioned over the region.** Quick, but it's a flat quad: no
  globe curvature, terrain conformance, or LOD; z-fights with the surface at grazing
  angles. Fine as a stopgap for a tiny flat region, wrong as the real feature.
- **Bake the extent into the video's alpha and keep using whole-globe
  `VideoTileProvider`.** Wastes texture/bandwidth on a mostly-empty global frame and
  still can't georeference precisely. Rejected.

---

## 9. Estimated work

| Piece | Effort | Risk |
|---|---|---|
| `GeodeticPatch::overlaps` + unit test | trivial | low |
| `RegionalVideoTileProvider` (clone VideoTileProvider + extent UV math) | ~1 day | low |
| Transparent-border / alpha-blend confirmation (or shader-clip fallback) | ~0.5–1 day | **medium** (the one real unknown) |
| `Extent` parameter + codegen + asset doc | ~0.5 day | low |
| `maxLevel` heuristic | ~0.5 day | low |
| Tests (UV mapping, culling) + sample asset | ~0.5 day | low |

Critical path is the §6.1 border/alpha validation; everything else is mechanical reuse of
existing tile-provider and `VideoPlayer` infrastructure.

---

## 10. File touch-list

- **New**: `modules/video/include/regionalvideotileprovider.h`,
  `modules/video/src/regionalvideotileprovider.cpp`
- **Edit**: `modules/video/src/videomodule.cpp` (register class),
  `modules/video/CMakeLists.txt` (add sources)
- **Edit**: `modules/globebrowsing/src/geodeticpatch.h/.cpp` (add `overlaps`)
- **Edit (only if §6.1 fallback needed)**:
  `modules/globebrowsing/shaders/texturetilemapping.glsl` (+ the layer uniform struct in
  `tile.glsl`) for per-fragment extent clip
- **New test**: `modules/globebrowsing/tests/` (or the existing test target) for the
  extent→UV mapping and overlap predicate
- **New sample asset** demonstrating a regional video overlay (drive from the Digital
  Earth Energy data)
