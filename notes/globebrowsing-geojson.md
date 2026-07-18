# OpenSpace GlobeBrowsing — GeoJSON layers

Notes on the GeoJSON ("Globe Geometry") feature of OpenSpace's **GlobeBrowsing** module: how it
renders vector geographic data on a globe, how to add it, and the full styling-property reference.

> Compiled 2026-06-20 from the OpenSpace docs (`docs.openspaceproject.com/latest`) and the
> `modules/globebrowsing/src/geojson/` source on GitHub (`OpenSpace/OpenSpace@master`). OpenSpace is
> the astrovisualization application (openspaceproject.com) — *not* the unrelated "OpenSpace" AI
> agent platform that Context7 indexes. Property defaults/enums below are quoted from source and may
> shift between releases (current stable line: v0.21 / v0.20); re-verify against the version you run.

## 1. What it is

GeoJSON files loaded into GlobeBrowsing are variously called **GeoJson Layers**, **Globe Geometry
Layers**, or **GeoJson Components**. They render the three standard GeoJSON geometry families —
**points, lines (LineString), and polygons** — draped onto the surface of any globe (planet, moon,
etc.). GeoJSON encodes geographic structures from 2D or 3D coordinates: **longitude, latitude, and
an optional altitude**.

Distinct from raster **tile layers** (imagery/height maps painted on tiles): GeoJSON features are
*vector geometry* triangulated/extruded into actual meshes sitting on (or above) the surface.

## 2. Architecture (source: `modules/globebrowsing/src/geojson/`)

| Class / file | Role |
|---|---|
| `GeoJsonComponent` | One loaded GeoJSON layer attached to a globe; owns the features + a `GeoJsonProperties` style block. Appears in the Scene tree under the globe's **GeoJson** group. |
| `GeoJsonManager` | Per-globe registry of components; backs the Lua `addGeoJson`/`deleteGeoJson` calls. |
| `GlobeGeometryFeature` | One feature within a component. Parses GEOS geometry, triangulates polygons, tessellates lines, drapes onto the height map, manages GPU buffers, renders. |
| `GeoJsonProperties` | The style/appearance properties (color, width, extrude, altitude mode, tessellation…) — settable in the asset, in the `.geojson` per-feature `properties`, or live in the UI. |

Parsing uses the **GEOS** library (`createFromSingleGeosGeometry()`), so standard GeoJSON validity
rules apply.

### GlobeGeometryFeature internals
- `GeometryType` enum: `LineString (0)`, `Point (1)`, `Polygon (2)`, `Error` (fallback) → mapped to
  `RenderType` Lines / Points / Polygon / Uninitialized.
- **Polygons** are triangulated and support **holes (inner rings)**; coordinates store the outer
  ring first, then inner rings.
- **Height mapping**: features sample the globe's height map dynamically via
  `bufferDynamicHeightData()` / `updateHeightsFromHeightMap()`, re-buffering when the heightmap
  tile changes (tracked by reference points + timestamps). This is why a feature can be hidden
  *inside* terrain and needs a height offset to surface (see §6).
- **Tessellation**: `tessellationStepSize()` sets vertex spacing along edges so straight GeoJSON
  segments bend to follow globe curvature.
- **Two-pass render**: `render()` does multi-pass rendering with culling control for correct
  polygon fill.

## 3. Three ways to add a GeoJSON layer

1. **Asset (preferred)** — load the file in a `.asset` and attach it to a globe at startup via
   `openspace.globebrowsing.addGeoJson`. Reproducible; lets you set styling + the target globe.
2. **`addGeoJsonFromFile`** — adds a file to the **current focus node** if it is a globe.
3. **Drag-and-drop** — drop a `.geojson` onto the OpenSpace window while a globe is focused. Missing
   properties take default values.

### Lua / asset API (source: `globebrowsingmodule_lua.inl`)

```lua
-- Add a configured layer to a specific globe (table form)
openspace.globebrowsing.addGeoJson(globeIdentifier, table)
--   globeIdentifier : scene-graph node id of the globe, e.g. "Earth"
--   table           : GeoJson layer config (see fields below)

-- Add a file to the current anchor node (if it's a globe). `name` is the optional UI label.
openspace.globebrowsing.addGeoJsonFromFile(filename, name)
--   NOTE (verbatim): "you might have to increase the height offset for the added feature to be
--   visible on the globe, if using a height map."

-- Remove a layer by its table or by its string Identifier
openspace.globebrowsing.deleteGeoJson(globeIdentifier, tableOrIdentifier)
```

`addGeoJsonFromFile` auto-generates a dictionary with `Identifier` (from the filename stem or the
provided `name`), `File` (the path), and optional `Name`.

### Asset example (table passed to `addGeoJson`)

The config table identifies the layer and points at the file; `DefaultProperties` supplies styling
for any feature that doesn't carry its own `properties` in the GeoJSON:

```lua
local Layer = {
  Identifier = "MyCountries",
  Name = "Country Outlines",
  File = asset.resource("data/countries.geojson"),
  Opacity = 1.0,
  HeightOffset = 10000.0,            -- metres above the surface/height map
  DefaultProperties = {
    Color      = { 1.0, 0.8, 0.0 }, -- line / point color
    FillColor  = { 0.2, 0.2, 0.5 }, -- polygon fill
    FillOpacity = 0.5,
    LineWidth  = 2.0,
    Extrude    = false,
    AltitudeMode = "RelativeToGround",
    Tessellation = { Enabled = true }
  }
}

asset.onInitialize(function()
  openspace.globebrowsing.addGeoJson("Earth", Layer)
end)
asset.onDeinitialize(function()
  openspace.globebrowsing.deleteGeoJson("Earth", Layer.Identifier)
end)
```

Bundled, runnable examples live in `data/assets/examples/geojson/` of any OpenSpace install.

### Collection-wide helper properties
To tune a whole layer at once (rather than per feature): **Opacity**, **Height Offset**,
**Point Size Scale**, **Line Width Scale**.

## 4. Styling properties (`GeoJsonProperties`, verbatim from source)

Set per-feature inside the GeoJSON `"properties"` object, or as `DefaultProperties` in the asset,
or interactively in the UI. Names below are the exact property identifiers.

| Property | Default | Type / range | Meaning |
|---|---|---|---|
| `Opacity` | `1.0` | float 0–1 | Overall feature opacity. |
| `Color` | `(1,1,1)` | vec3 | Geometry color; for points, the point color; for lines, the line color. |
| `FillOpacity` | `0.7` | float 0–1 | Opacity of the filled part of a polygon. |
| `FillColor` | `(0.5,0.5,0.5)` | vec3 | Fill color of a rendered polygon. |
| `LineWidth` | `2.0` | float 0.01–10 | Width of rendered lines. |
| `PointSize` | `10.0` | float 0.01–100 | Point size (scaled by distance). |
| `PointTexture` | *(none)* | string | Texture for points; empty → default point sprite. |
| `PointTextureAnchor` | `Bottom` | enum: `Bottom (0)`, `Center` | Where the texture anchors relative to the point position. |
| `Extrude` | `false` | bool | Extrude geometry down to intersect the globe (walls/volumes from outlines). |
| `PerformShading` | `false` | bool | Apply lighting/shading to generated meshes. |
| `AltitudeMode` | *(option)* | enum: `Absolute (0)`, `RelativeToGround` | How altitude/height values are interpreted (see §5). |
| `Tessellation.Enabled` | `true` | bool | If false, no curvature-bending tessellation. |
| `Tessellation.UseTessellationLevel` | `false` | bool | Use a manual level instead of distance-based. |
| `Tessellation.TessellationLevel` | `10` | int 0–100 | Subdivision count when manual tessellation is on. |
| `Tessellation.TessellationDistance` | `100000.0` | float (m) | Default edge distance used to tessellate lines. |

Member fields (source `geojsonproperties.h`): `tessellation`, `opacity`, `color`, `fillOpacity`,
`fillColor`, `lineWidth`, `pointSize`, `pointTexture`, `pointAnchorOption`, `extrude`,
`performShading`, `altitudeModeOption`.

## 5. Altitude / height handling

`AltitudeMode` decides how the (optional) GeoJSON altitude is placed:
- **`Absolute`** — altitude above the reference **ellipsoid** (ignores terrain).
- **`RelativeToGround`** — altitude above the **height map** (terrain-following).

Because features sample the live height map, anything at altitude 0 with `RelativeToGround` can sit
*flush with or under* terrain. Use the layer's **`HeightOffset`** (and/or per-feature altitude) to
lift features clear of terrain — this is the single most common "my GeoJSON isn't showing" fix.

## 6. Point rendering modes

`PointRenderMode` (in `GlobeGeometryFeature`) controls how point sprites orient:
- `AlignToCameraDir (0)` — face the camera direction.
- `AlignToCameraPos (1)` — face the camera position.
- `AlignToGlobeNormal (2)` — align to the globe surface normal.
- `AlignToGlobeSurface (3)` — lie flat on the surface.

## 7. Performance

- Rendering degrades when a feature has **many triangles** (large/complex polygons) or a file has
  **many features**. Triangulation + per-feature height buffering dominate cost.
- Many simultaneous layers compound this — **deleting or simply disabling** layers you aren't
  viewing recovers frame rate.
- Tessellation level multiplies vertex counts; lower `TessellationLevel` / rely on distance-based
  tessellation for large datasets.

## 8. Managing layers in the UI

- Each component shows in the **Scene** menu under a **GeoJson** group nested beneath its parent
  globe; expand to toggle visibility and edit `GeoJsonProperties` live.
- Programmatic removal: `openspace.globebrowsing.deleteGeoJson(globe, identifier)`.

## 9. Field notes from this branch (2026-06/07)

- **Asset `File` paths must be resolved** — a bare relative string like
  `File = "foo.geojson"` fails codegen's file-existence verifier (`Verification failed:
  File ... did not exist` + `GeoJsonComponent: Error in specification`) because it
  resolves against the CWD, not the asset directory. Use
  `File = asset.resource("foo.geojson")`.
- When a component fails that verification it is **never registered**, so the asset's
  `onDeinitialize` → `deleteGeoJson` later logs `Could not find GeoJson layer <id>` at
  shutdown — a downstream symptom, not a second bug. (`GeoJsonManager::deleteLayer` was
  downgraded from LERROR to LWARNING on this branch for exactly this case.)
- `addGeoJsonFromFile` derives the identifier via `makeIdentifier(stem)`, which maps
  punctuation (except `-`/`_`) to `-`: `transmission_735kV+.geojson` →
  `transmission_735kV-`.
- **KML-derived GeoJSON property types** (fixed on this branch,
  `geojsonproperties.cpp`): KML→GeoJSON converters emit booleans as numbers
  (`"extrude": 1`, `"tessellate": -1`) and pad features with `null` properties
  (`"timestamp": null`). The per-feature property parser (`propsFromGeoJson`) used
  strict `getBoolean()`/`getNumber()`, throwing `GeoJSONTypeError` → logged as
  `Error reading GeoJson property '<key>'. Value has wrong type`. Now: `boolValue()`
  accepts bool/number (non-zero = true, incl. KML's -1)/string ("true"/"1"/"-1"),
  `numberValue()` accepts numeric strings, `null` values are skipped as absent, and
  error messages include the feature index, file name, and the offending value + its
  actual type (context threaded from `GeoJsonComponent::parseSingleFeature`).
  `colorValue` also no longer throws `std::out_of_range` on arrays shorter than 3.
- `GeoJsonComponent : PropertyOwner, Fadeable` → per-layer `Enabled`, `Fade`, `Opacity`
  exist in addition to `HeightOffset` and `DefaultProperties.*`; the per-globe owner is
  `Scene.<Globe>.Renderable.GeographicOverlays`.
- See `notes/openspace-web-api.md` for driving GeoJSON layers from the WebSocket API.

## 10. Rendering internals (source deep-dive, 2026-07-02)

Read from the actual source on this branch; line numbers refer to
`modules/globebrowsing/src/geojson/`.

### Shader programs (two, shared per component)
Built in `GeoJsonComponent::initializeGL` (geojsoncomponent.cpp:473-484):
- **`GeoLinesAndPolygonProgram`** — `shaders/geojson_vs.glsl` + `geojson_fs.glsl`. Used for
  lines, polygon fills, and extrusion walls.
- **`GeoPointsProgram`** — `geojson_points_{vs,fs,gs}.glsl`. Points are expanded to
  billboards in the geometry shader.

### Geometry pipeline (CPU)
1. **Parse** (`createFromSingleGeosGeometry`, globegeometryfeature.cpp:165-289): polygons
   are triangulated *at parse time* with GEOS `ConstrainedDelaunayTriangulator`
   (handles holes; winding flipped CW→CCW, lines 197-209). Ring outlines are kept
   separately in `_geoCoordinates` (outer ring first, then holes). Also computes
   `_heightUpdateReferencePoints` = centroid + envelope corners (lines 268-281).
2. **Build** (`updateGeometry`, lines 517-532) creates `RenderFeature`s:
   - lines: each ring → `GL_LINE_STRIP` vertices, tessellated by `subdivideLine`
     (fixed step = `tessellationStepSize()`);
   - extrusion walls: quads from ring edges (`createExtrudedGeometryVertices`);
   - polygon fill: each parsed triangle subdivided by `subdivideTriangle` →
     unindexed `GL_TRIANGLES` soup (no index buffer anywhere).
3. Every **final (post-tessellation) vertex** position is converted *back* to geodetic:
   `feature.vertices = geodetic2FromVertexList(...)` uses
   `ellipsoid().cartesianToGeodetic2(v.position)` per vertex
   (globegeometryhelper.cpp:81-90) — kept CPU-side purely so heights can be re-sampled.
   **This means per-vertex lat/lon already exists for every drawn vertex** (relevant for
   any future UV/texturing work, see `DESIGN_video_on_geojson_features.md`).

### GPU data layout (`initializeRenderFeature`, lines 759-816)
- VBO 0: `Vertex = rendering::VertexXYZNormal` `{ vec3 position; vec3 normal }`,
  attributes `in_position`, `in_normal`. Positions are **single-precision model-space**
  coordinates of the full-size globe (Earth radius ≈ 6.37e6 m → float mantissa gives
  ~0.5–1 m quantization; fine for visualization, explains why no sub-meter registration
  can be expected).
- VBO 1: `in_height` — one float per vertex, the sampled height-map height
  (`heightMapHeightsFromGeodetic2List` → `getHeightToReferenceSurface`). Re-uploaded with
  `glNamedBufferData(GL_DYNAMIC_DRAW)` on height-map change (`bufferDynamicHeightData`,
  lines 846-856) — vertex *positions* are never touched by height updates.
- Height update policy (`shouldUpdateDueToHeightMapChange`, lines 475-502): only in
  `RelativeToGround` mode, at most every **10 s** (`HeightUpdateInterval`, line 64), and
  only if the sampled heights at the reference points changed. The refresh
  (`updateHeightsFromHeightMap`, lines 534-542) re-samples **all** vertices in one frame
  (has a `@TODO` to amortize — a known hitch for huge features, cf. issue #2730).

### Vertex shader (geojson_vs.glsl:46-61)
Terrain conformance is done in the shader, not by rebuilding geometry:
`modelPos += normalize(in_position) * (useHeightMapData ? in_height + heightOffset : heightOffset)`.
So `HeightOffset` edits are free (uniform), and the displacement direction is the
*geocentric* out-direction, not the ellipsoid normal. `gl_Position.z` is forced to 0
(depth handled via `out_data.depth = w`, the ABuffer/fragment framework convention).

### Fragment shader (geojson_fs.glsl:47-74)
Flat `vec4(color, opacity)`; optional per-light Lambert shading (normals are inverted with
a `@TODO fix faulty triangle normals` note). **No texture sampling for lines/polygons** —
only the points program samples a texture (billboard sprite + `textureWidthFactor`
aspect correction, globegeometryfeature.cpp:424-431). Point size =
`0.001 * pointSizeScale * PointSize * globe.boundingSphere()` (lines 387-389).

### Draw pass structure
`GeoJsonComponent::render` (geojsoncomponent.cpp:512-565): standard
`SRC_ALPHA, ONE_MINUS_SRC_ALPHA` blending + depth test, then **two global passes** over
all features. Per feature (`GlobeGeometryFeature::render`, lines 291-377):
- pass 1 is skipped unless `shouldRenderTwice = polygon && fillOpacity < 1 && Extrude` —
  in that case pass 0 draws back faces, pass 1 front faces (`glCullFace`,
  renderPolygons lines 464-471) so translucent extruded volumes composite correctly.
- Extrusion features draw with `fillColor`/`fillOpacity`; outline lines with
  `color`/`opacity`.
- Each `RenderFeature` is its own VAO + `glDrawArrays` call with full uniform re-upload —
  many-feature files pay per-feature program state cost (another #2730 angle).

### Where it hooks into the globe
`GeoJsonManager` is a `PropertyOwner` ("GeographicOverlays") owned by `RenderableGlobe`;
its components render **after** the globe's tile chunks in the same render task, i.e.
vector features composite over the raster layer stack with regular GL blending (not part
of the per-tile layer shader).

## Sources
- [Globe Geometry Features from GeoJson Files — OpenSpace docs (latest)](https://docs.openspaceproject.com/latest/building-content/globebrowsing/geojson-layers.html)
- [Adding Geometry with GeoJson — OpenSpace docs (v0.21)](https://docs.openspaceproject.com/releases-v0.21/building-content/globebrowsing/creation/adding-geojson-layers.html)
- [GeoJsonProperties — OpenSpace docs (generated component reference)](https://docs.openspaceproject.com/en/latest/generated/asset-components/GeoJsonProperties.html)
- Source: `modules/globebrowsing/src/geojson/{geojsonproperties.cpp,geojsonproperties.h,globegeometryfeature.h}` and `globebrowsingmodule_lua.inl` — [OpenSpace/OpenSpace@master](https://github.com/OpenSpace/OpenSpace)
