# Changelog

All notable changes made on the `project/geojson_perf` branch (relative to `master`).
Format loosely follows [Keep a Changelog](https://keepachangelog.com/).

## [Unreleased] — 2026-07-03

### Added
- **Native in-app show controls** — the entire `de_energy_0.21.html` dashboard (14
  cards, 164 buttons) ported to OpenSpace actions:
  `data/assets/digital_earth/ENERGY_I/actions/*.asset` register everything in the
  Actions panel under `/Digital Earth/<NN Section>` folders (with the panel's color
  coding as `Color` hints and `IsLocal = false` for cluster/parallel sync), and
  `mission.asset` adds a "Digital Earth: Energy I" entry to the Missions panel — five
  narrative energy-era phases (whale oil → coal → oil & gas → electricity → the grid
  today) with per-phase action buttons, era time ranges, milestones, and slide-icon
  imagery. LOAD actions got idempotency guards (`hasProperty` before
  `addGeoJsonFromFile`) the HTML buttons lacked. The profile loads `mission` (which
  requires the actions) and binds CTRL+F12 to the ported basic-setup action. The HTML
  panel remains fully functional in parallel at `/panels`.

### Fixed
- **Broken JS-syntax profile actions** — `de_energy_0.21.profile` contained two actions
  whose "Lua" scripts were pasted JavaScript (`var`/`await`/JS arrays):
  `key.f12.dmns.setup` (CTRL+F12, superseded by the ported
  `de.energy.setup.de_basic_setup_ctrl_f12`) and
  `key.alt.x.earth.earthatm.stars.sun.toggle` (rewritten in Lua; its
  `boolean[1] > 0.1` condition was broken even as JS).

## [Unreleased] — 2026-07-02

### Fixed
- **GeoJSON per-feature property parsing (KML-derived files)** — loading
  `us_states_50m-admin1_no_labels.geojson` errored with `Error reading GeoJson property
  'extrude'/'tessellate'. Value has wrong type`: KML→GeoJSON conversions encode booleans
  as numbers (`1`, `-1`, `0`) and include `null` properties, but `propsFromGeoJson`
  (`geojsonproperties.cpp`) used strict `getBoolean()`/`getNumber()`. Boolean-typed
  properties now accept numbers (non-zero = true) and strings ("true"/"1"/"-1");
  number-typed properties accept numeric strings; `null` values are skipped as absent.
  Error reporting now includes the feature index, file name, and the offending value
  with its actual type (context threaded from `GeoJsonComponent::parseSingleFeature`),
  and `colorValue` no longer throws `std::out_of_range` on malformed color arrays.
  Audited all 40+ geojson files used by the control panel: only the us_states file
  carried these KML-style properties; the rest only use string `name`s.

### Added
- **Web panel hosting** — `WebGuiModule` now accepts a `Directories` key in its
  `openspace.cfg` configuration (endpoint/directory pairs) that seeds
  `Modules.WebGui.Directories` before `util/webgui.asset` appends its own endpoints, so
  static content can be added to the (single) WebGui http server from the config file.
  The default config serves `${DATA}` at the `panels` endpoint with `HttpPort = 4690`,
  hosting the control panel at `http://localhost:4690/panels/` while OpenSpace runs (no
  more `file://`); the GUI lives on the same port at `/gui`. `data/index.html` redirects
  the endpoint root to `de_energy_0.21.html` (the backend otherwise answers
  "Cannot GET /panels/" for directories without an index). The panel's `mapButtons` now
  emits page-relative URLs for button-label images when served over HTTP (browsers block
  `file:///` loads from an http origin) while keeping filesystem paths in the scripts
  sent to OpenSpace.

## [Unreleased] — 2026-06-22 → 2026-07-01

Digital Earth "Energy I" show content moved into the repo, its web control panel repaired
against the current WebSocket API, and assorted branch fixes.

### Added
- **Digital Earth Energy I content** relocated from the gitignored `user/` tree into the
  repo: assets/media under `data/assets/digital_earth/ENERGY_I/`, profiles
  (`de_energy_0.21`, `custom_test`, `smaller`) under `data/profiles/`, the session
  recording under `data/recordings/`, and the control panel
  (`de_energy_0.21.html`, `openspace-api.js`, `main2.css`) under `data/`.
  mp4/mp3 media (~920 MB; four files exceed GitHub's 100 MB limit) is **gitignored and
  kept local-only** — the account's Git LFS budget was exhausted, so LFS was backed out.
- **`build-llvm.sh`** — Git Bash wrapper for the `windows-llvm` preset; runs
  configure/build inside `vcvars64` (required: clang-cl needs the Windows SDK and the
  webbrowser POST_BUILD manifest step needs the SDK's `mt.exe` on PATH). Handles the
  cmd.exe nested-quote and MSYS `/c`→path-conversion pitfalls via a temp `.bat` +
  `MSYS_NO_PATHCONV=1`.
- `notes/openspace-web-api.md` — field notes on the WebSocket API (apiHandshake
  requirement, return-value unwrapping, renamed Lua functions, path tokens, audio module,
  property-URI reference, debugging workflow).

### Fixed
- **Web panel `de_energy_0.21.html`** (was completely non-functional):
  - auto-connect ignored its host argument and read an empty input → connected to
    `ws://:4682`; now falls back `arg → input → 'localhost'`.
  - replaced the v0.21-era `openspace-api.js` with the current `openspace-api-js` build —
    the server now requires an `apiHandshake` first message and killed old clients with
    "Unsupported API version".
  - removed all old-style `result[1]` / `result["1"]` unwrapping (the new client returns
    values directly); this had silently corrupted `openSpaceRoot` (→ `":/"`) breaking all
    `ROOT/`-injected image/layer paths, and stuck every state-dependent toggle on one
    branch.
  - `openspace.getPropertyValue` → `openspace.propertyValue` (renamed upstream);
    `RenderEngine.BlackoutFactor` → `RenderEngine.GlobalBlackout.Factor` (moved upstream).
  - remapped Oil-Drilling / Sailing-songs buttons from removed `ScreenSpace.*Video`
    nodes to `openspace.audio.*` (their assets now play extracted mp3s — the source mp4s'
    16x16 dummy video stream is undecodable by libmpv).
  - removed stale references: `ColorLayers.us_coal_power_plants_1935_2028` (layer no
    longer exists) and the dead `allslidesdown()` helper (ScreenSpace ids from a
    different show).
- **`us_states_lines.asset`** — `File` was a bare relative string, which fails the
  codegen file-existence verifier (resolves against CWD, not the asset dir); now
  `asset.resource("us_states_50m-admin1_no_labels.geojson")`.
- **US_EIA_OPERATING_GENERATORS assets** — 10 assets hardcoded a dead absolute path from
  an old install (`C:/OpenSpace/OpenSpace-0.21.2/...`); now `asset.resource(...)`.
- **`GeoJsonManager::deleteLayer`** — layer-not-found downgraded LERROR → LWARNING; a
  component that failed spec verification is never registered, so its asset's
  `onDeinitialize` delete is an expected miss, not an error.

### Changed
- **`windows-min` / `windows-llvm` presets** — `OPENSPACE_MODULE_VIDEO=ON` (the Energy
  show uses ScreenSpaceVideo).
- Moved-content path rewrites: `${USER_ASSETS}` → `${ASSETS}` in the panel, the ENERGY_I
  assets, and the profiles; `ROOT/user/data/assets/` → `ROOT/data/assets/`;
  `${RECORDINGS}/…` → `${DATA}/recordings/…` in the panel.
- `modules/webbrowser/cmake/cef_support.cmake` — documented why the manifest step must
  use the SDK `mt.exe` (llvm-mt cannot merge two manifests) and therefore run inside a
  VS dev environment.

## [Unreleased] — 2026-06-17 → 2026-06-20

Work on this branch falls into two themes: (1) standing up a Windows LLVM/clang-cl
toolchain alongside the existing MSVC build, and (2) groundwork for geoJSON/GDAL features,
including a deterministic-interpolation hook and a crash fix surfaced while building tests.

### Added
- **Windows LLVM build (`windows-llvm` preset)** — clang-cl + lld-link via the
  `Ninja Multi-Config` generator, configured to coexist with the MSVC build. Companion
  `build-llvm.bat` runs any command inside the VS18 `vcvars64` dev environment so Ninja
  gets the Windows SDK / MSVC headers and libs.
- **`windows-min` preset** — minimal Visual Studio build (no tests, no vcpkg, reduced
  modules, warnings off) for fast local iteration.
- **`Scene::setInterpolationTimeReference()`** — overrides the property-interpolation
  clock with a fixed `steady_clock::time_point` (or `std::nullopt` to restore the real
  clock), making interpolation behavior deterministic for tests.
- **`compile_commands.json` export** enabled for clangd/LSP tooling.

### Fixed
- **Crash in `PropertyOwner::removePropertySubOwner`** — now null-checks
  `renderEngine->scene()` before walking interpolations, which could be absent during
  shutdown or in tests (no active scene).
- **fitsfilereader** — pass `path.string()` to the CCfits `FITS` constructor, which wants
  `std::string` rather than `std::filesystem::path` (clang rejected the implicit
  conversion).
- **webbrowser process helper** — replaced the non-standard `WinMain`-signature `main`
  with a standard `int main(int, char**)` (the helper is a console-subsystem exe), getting
  the module instance from `GetModuleHandle(nullptr)`; clang strictly validates `main`'s
  signature where MSVC did not.

### Build / Toolchain (clang-cl strictness fixups; MSVC builds unaffected)
- **spice** — suppress legacy f2c K&R-C diagnostics that clang 16+ promotes to errors
  (`-Wno-implicit-int`, etc.) and disable the forced `SpiceZfc.h` PCH (conflicting f2c
  prototypes). Applied per-target so CMake's clang-cl `CMAKE_C_FLAGS` defaults (`/DWIN32`
  …, relied on by the CDF library) are not clobbered. *(main repo `CMakeLists.txt`)*
- **assimp** — rewrite the generated `revision.h` copyright string from the raw `\xA9`
  (©) byte to ASCII `(C)`; `llvm-rc` rejects the non-ASCII byte in a non-Unicode
  VERSIONINFO string. *(routed through the ghoul submodule, branch `benpm/dev`)*
- **tiny-process-library** — overwrite the target's compile definitions to the bare
  `_CRT_SECURE_NO_WARNINGS`; its CMake passed `/D_CRT_SECURE_NO_WARNINGS` to
  `target_compile_definitions()`, producing the invalid `-D/D_CRT…`. *(ghoul submodule,
  branch `benpm/dev`)*
- **sgct `image.cpp`** — hoist the stb includes out of an anonymous namespace to global
  scope with `STB_IMAGE_STATIC` / `STB_IMAGE_WRITE_STATIC` (internal linkage); inside the
  namespace, stb's `<stddef.h>` nested `namespace std` and made every `std::` ambiguous
  under clang. *(sgct submodule, branch `benpm/dev`)*
- **OpenSpace link** — add `/force:multiple` under clang-cl; zlib is bundled twice
  (standalone `zlibstatic` and inside CDF/kameleon), so symbols like `z_errmsg` are
  defined twice — MSVC's linker resolves via archive-member selection, lld-link errors.

### Changed
- Project notes reorganized under `notes/` (`TODO.md`, `ISSUE_DRAFTS.md`,
  `DESIGN_regional_video_overlays.md`).

### Tests
- **Deterministic interpolation tests** — `test_lua_setpropertyvalue` drives
  `Scene::setInterpolationTimeReference()` with a virtual time so interpolated
  `setPropertyValue` behavior is reproducible without depending on wall-clock timing.
- **Profile tests** — hardened with `PathTokenPushPopStack` for per-case path-token
  isolation, plus additional profile cases.

### Docs / Project notes
- `notes/TODO.md` — tracked geoJSON/GDAL tasks (e.g. layers disappearing after long
  runtime).
- `notes/ISSUE_DRAFTS.md` — draft GitHub issue descriptions for the TODO items.
- `notes/DESIGN_regional_video_overlays.md` — design for geospatially-bounded video and
  animated raster overlays on globe surfaces.
- `.claude/CLAUDE.md` — project instructions covering the LLVM/clang toolchain, the
  preset workflow, and the required submodule fixups.

### Submodule branches
- `ext/ghoul` → `benpm/dev`: clang-cl fixups for its nested assimp and
  tiny-process-library submodules (their own working trees left pristine).
- `apps/OpenSpace/ext/sgct` → `benpm/dev`: the `image.cpp` stb fix.
