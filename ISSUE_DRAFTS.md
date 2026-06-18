# Draft GitHub Issues

Draft descriptions for the [TODO.md](TODO.md) tasks that do **not** yet have a precise
dedicated issue in [OpenSpace/OpenSpace](https://github.com/OpenSpace/OpenSpace). Copy
each block into a new issue as-is (or trim) when filing. Repro data referenced below is the
*Digital Earth Energy* package:
[DIGITAL_EARTH_ENERGY.zip](https://www.dropbox.com/scl/fi/uv7rd7lto4itlmtz6y6ty/DIGITAL_EARTH_ENERGY.zip?rlkey=l905fa217yzzgbgkhc31azskv&st=gkk72qv1&dl=0)
(extract into the `user/` folder; drive it from the top-row LOAD/SETUP buttons in the
`HTML_BROWSER_CONTROLS/` page).

> **Already tracked — no new issue needed:**
> - "Make GeoJSON rendering more efficient by loading only visible primitives" →
>   [#2730 Improve performance of GeoJson rendering](https://github.com/OpenSpace/OpenSpace/issues/2730)
> - "Finish postprocessing pipeline work" →
>   [#3828 Robust Postprocessing Support](https://github.com/OpenSpace/OpenSpace/issues/3828)

---

## 1. GDAL raster layers stop rendering or render only partially after extended runtime

**Labels:** `bug`, `module: globebrowsing`, `GDAL`

### Summary
GDAL-backed image layers load and render correctly when first enabled, but after OpenSpace
has been running for a while they stop appearing entirely, or render only partially —
typically visible only in the northeastern corner of the continental US while remaining
invisible elsewhere.

### Steps to reproduce
1. Extract the Digital Earth Energy package into `user/` and open the
   `HTML_BROWSER_CONTROLS/` page.
2. Click through the top-row LOAD and SETUP buttons in order to load and configure the
   datasets.
3. Enable the GDAL raster layers (Row 8 in the control page).
4. Leave OpenSpace running and continue interacting (navigate, toggle layers) for an
   extended period.

### Current behavior
- The layers render at first.
- After some time they disappear, or only partially render (e.g., a patch over the
  northeastern US shows while the rest of the layer stays blank).

### Expected behavior
- Enabled GDAL raster layers continue to render fully and consistently regardless of how
  long OpenSpace has been running.

### Notes / hypotheses to investigate
- Tile cache evicting or invalidating tiles that hold valid data.
- GDAL dataset handle exhaustion or a leak over the session lifetime.
- Async tile loading silently failing after running for a while (worker pool / file
  handle limits).
- Memory pressure causing tiles to be dropped and not re-requested.

### Related
- [#3869 Temporal layer caches unavailable data](https://github.com/OpenSpace/OpenSpace/issues/3869)
- [#386 Make temporal tile layers independent of GDAL](https://github.com/OpenSpace/OpenSpace/issues/386)
- [#3031 Earth missing tiles on startup](https://github.com/OpenSpace/OpenSpace/issues/3031) (closed)

---

## 2. Geospatial image layers loaded as `.vrt` (GDAL VRT) datasets render incorrectly

**Labels:** `bug`, `module: globebrowsing`, `GDAL`

### Summary
Geospatial image layers supplied as `.vrt` (GDAL ViRTual dataset) files do not render
correctly through the globebrowsing GDAL tile path. This issue tracks investigating and
fixing how `.vrt` datasets are read and rasterized.

### Steps to reproduce
1. Add a globebrowsing image layer whose source is a `.vrt` file (e.g., a VRT mosaic of the
   geospatial rasters in the Digital Earth Energy package).
2. Enable the layer on the Earth globe.

### Current behavior
- The `.vrt`-backed layer does not render as expected (to be characterized: missing tiles,
  incorrect transparency/NoData handling, color/stretch differences, or misalignment vs the
  equivalent non-VRT source).

### Expected behavior
- A `.vrt` layer renders identically to the same data loaded directly (correct extent,
  resolution, color, and transparency).

### Notes / angles to investigate
- NoData / alpha-channel handling for VRT bands (see #1243).
- VRT band/overview selection and resampling through the GDAL reader.
- Caching behavior for VRT sources (see #1588).
- Capture concrete examples (a `.vrt` that fails + a screenshot) to pin the exact failure
  mode.

### Related
- [#1243 Add support for understanding NoData values in OpenSpace to not use the Alpha channel for that](https://github.com/OpenSpace/OpenSpace/issues/1243)
- [#1588 VRT layers are not cached with WMSCacheEnabled](https://github.com/OpenSpace/OpenSpace/issues/1588) (closed)

---

## 3. Support geospatially-bounded video and animated raster overlays on globe surfaces

**Labels:** `feature`, `module: globebrowsing`, `module: video`

### Summary
Today a video layer covers the entire globe. We want the ability to place a video — or an
animated raster/map sequence — over only **part** of a globe's surface (a geographic extent
/ bounding box), and more generally to geospatially place movies onto planetary surfaces.
The driving use case is overlaying animated geospatial maps (e.g., time-varying
energy/weather datasets) onto a sub-region of Earth alongside the existing static imagery.

### Motivation
- Animated datasets are naturally regional (e.g., a US-only animated map); stretching them
  across the whole globe is wrong and wasteful.
- Presentations need to mix a static base map with one or more animated overlays confined
  to their real-world extent.

### Proposed scope
- A globebrowsing layer type backed by video (or an image sequence) that is clipped to a
  configurable geographic extent (lat/lon bounding box), composited over lower layers.
- Correct alignment with existing imagery layers and the globe's coordinate system.
- Per-overlay time control (start/end, looping, playback rate) tied to the application or
  simulation time.
- Support across planetary bodies, not just Earth.

### Acceptance criteria
- An asset can declare a video/animated overlay with a bounding extent and it renders only
  within that extent, correctly georeferenced.
- Multiple such overlays can be enabled simultaneously without bleeding outside their
  extents.
- Playback is controllable and stays in sync in single- and clustered-window setups.

### Related
- [#3846 Mismatching video / NOAA SOS layer on Earth](https://github.com/OpenSpace/OpenSpace/issues/3846)
- [#4025 Globe Imagery Browser should add Temporal layers when possible](https://github.com/OpenSpace/OpenSpace/issues/4025)
- [#2830 Create Globebrowsing support scripts for NOAA Temporal datasets via GDAL](https://github.com/OpenSpace/OpenSpace/issues/2830)
- [#2683 Video player on globe rendering flickering](https://github.com/OpenSpace/OpenSpace/issues/2683) (closed)
- [#2684 Add video layer to Jupiter for Juno profile](https://github.com/OpenSpace/OpenSpace/issues/2684) (closed)

> Covers TODO items "layer animations over part of the Earth / movies onto planet
> surfaces" and the two near-duplicate "overlaying geospatial animated maps onto Earth's
> surface" lines — these are one feature and should be filed as a single issue.
