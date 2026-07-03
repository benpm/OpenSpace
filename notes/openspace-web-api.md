# OpenSpace WebSocket API & web panels — field notes

Hard-won debugging knowledge from getting `data/de_energy_0.21.html` (Digital Earth Energy
control panel) working against this branch. Compiled 2026-07-01.

## 1. How a web panel connects

- A panel is a plain HTML file (opened via `file://` in any browser) that loads
  `openspace-api.js` and calls `window.openspaceApi(host, port)` →
  `api.onConnect(...)` → `api.connect()` → `openspace = await api.library()`.
- It talks to the **Server module's WebSocket interface**, configured in `openspace.cfg`
  (`Server.Interfaces`, `Type = "WebSocket"`): default port **4682**, `DefaultAccess =
  "Deny"` with `AllowAddresses = { "127.0.0.1", "localhost" }`, empty password. The
  `ModuleConfigurations.WebGui.WebSocketInterface` key binds it. (Port 4681 is the raw
  TCP-socket twin; 4680 is the WebGui HTTP port.)
- `api.library()` fetches the Lua documentation and generates one async JS function per
  documented `openspace.*` Lua function, including Lua-side helpers defined in
  `scripts/core_scripts.lua` (`toggleFade`, `fadeIn`, `fadeOut`, ...).

## 2. The apiHandshake requirement (breaks old clients)

`src/topic/connection.cpp` (`Connection::handleJson`): the **first message** on a new
connection MUST be `{"type": "apiHandshake", "apiVersion": {major, minor, patch}}`.
Anything else → server logs `Unexpected error Unsupported API version, from: ...` and
**destroys the socket**. The server records the version but does not reject on the
numbers (`SOCKET_API_VERSION` is 0.1.0 in `include/openspace/topic/server.h`).

- The old (Dec 2023 / v0.21-era, ~99 KB webpack bundle) `openspace-api.js` never sends a
  handshake → every connection dies immediately after open. Symptom in the log:
  `onOpen` → `Unsupported API version` → `Destroying socket connection`.
- The current client is `OpenSpace/openspace-api-js` on GitHub — `dist/openspace-api.js`
  (~17 KB, plain IIFE). It auto-sends the handshake on socket open, *before* the
  user `onConnect` callback fires.
- **Browser caching gotcha**: after swapping the js file on disk, the browser keeps
  serving the cached old one. Bump the include URL
  (`<script src="openspace-api.js?v=YYYYMMDD">`) to force a re-fetch.

## 3. Return values: the new client unwraps, the old one didn't

Old client: every call resolved to the raw Lua table, so panels wrote
`(await openspace.absPath(...))["1"]` or `result[1]`.

New client (`library()`): `return luaTable[1]` — values arrive **already unwrapped**
(string/number/boolean directly). Any leftover `result[1]` / `result["1"]` now indexes
*into the value*: on a string it silently yields one character (e.g. `"C:\\..."[1]` →
`":"`), on a boolean it yields `undefined`. This produces silent logic breakage, not
errors — toggles stuck on one branch, paths reduced to `":"`.

## 4. Renamed / moved Lua API on this branch (vs v0.21 panels)

| Old | Current | Where defined |
|---|---|---|
| `openspace.getPropertyValue(uri)` | `openspace.propertyValue(uri)` | `src/scene/scene.cpp` (`Scene::luaLibrary`) |
| `RenderEngine.BlackoutFactor` (property) | `RenderEngine.GlobalBlackout.Factor` (+ `.Color`, `.ImageFactor`, `.ImagePath`) | `src/rendering/renderengine.cpp` |
| `ScreenSpace.<X>.VideoPlayer.*` for audio-only mp4s | replaced with `openspace.audio.*` playback | see §6 |

`setPropertyValue` / `setPropertyValueSingle(uri, value, duration?, easing?, postscript?,
isBouncing?)` are unchanged and are registered manually (not via codegen), so grepping
`*_lua.inl` won't find them — look in `Scene::luaLibrary()`.

## 5. Path tokens in API calls

- Functions taking `std::string` paths (e.g. `globebrowsing.addGeoJsonFromFile`) call
  `absPath()` server-side, so `${ASSETS}/...`, `${DATA}/...`, `${RECORDINGS}/...` tokens
  can be passed **directly** from JS — no client-side `absPath` round-trip needed.
- Token map (openspace.cfg `Paths`): `DATA=${BASE}/data`, `ASSETS=${DATA}/assets`,
  `USER=${BASE}/user` (or `OPENSPACE_USER` env), `USER_ASSETS=${USER}/data/assets`,
  `RECORDINGS=${USER}/recordings`, `SCREENSHOTS=${USER}/screenshots`.
- Profile asset lists resolve against **both** `${ASSETS}` and `${USER_ASSETS}`, so assets
  can move between the two trees without touching profile entries that use bare relative
  names. Entries with an explicit `${USER_ASSETS}/...` prefix must be rewritten.
- **Windows gotcha**: file lookups are case-insensitive, so a wrong-case path works
  locally but the derived identifier keeps the *typed* case (and Linux would fail).

## 6. Audio module (replacement for audio-only ScreenSpaceVideo)

mp4s that are audio + a 16x16 dummy video stream fail in libmpv (black square). Extract
to mp3 and use the audio module (`modules/audio/audiomodule_lua.inl`):

- `openspace.audio.playAudio(path, identifier, shouldLoop=true)` — **errors if the
  identifier is already live** ("Sound with name ... already played"); call
  `stopAudio(id)` first for play-from-start semantics.
- `stopAudio`, `pauseAudio`, `resumeAudio`, `setLooping`, `isPlaying`, `isPaused` are all
  **safe no-ops / false on unknown identifiers** (checked `audiomodule.cpp`).
- Also: `stopAll`, `pauseAll`, `resumeAll`, `playAllFromStart`, `setVolume`,
  `setGlobalVolume`, `playAudio3d(path, id, vec3 position, loop)`.

## 7. GeoJSON layers from the API

- `openspace.globebrowsing.addGeoJsonFromFile(filename, name?)` adds to the **current
  anchor node** (must be a RenderableGlobe). Identifier =
  `makeIdentifier(name or filename stem)`.
- **`makeIdentifier` maps punctuation (except `-`/`_`) to `-`** (`src/scene/scene.cpp`):
  `transmission_735kV+.geojson` → identifier `transmission_735kV-`. Keep this in mind
  when addressing the layer's properties afterwards.
- Properties live under `Scene.<Globe>.Renderable.GeographicOverlays.<identifier>.*`
  (the manager PropertyOwner is `GeographicOverlays`): `Enabled`, `Fade`, `Opacity`
  (Fadeable), `HeightOffset`, `DefaultProperties.{Color, FillColor, FillOpacity,
  LineWidth, PointSize, Extrude, PerformShading, AltitudeMode, ...}`.
- `deleteGeoJson(globe, tableOrIdentifier)`; delete/property calls on a layer that never
  loaded log "Could not find GeoJson layer ..." (downgraded to warning on this branch,
  `geojsonmanager.cpp`).

## 8. Misc property-URI facts verified against source

- `openspace.toggleFade(ownerUri, fadeTime?, endScript?)` needs the owner to have both
  `Enabled` and `Fade`; called on a *group* owner (e.g. `...GeographicOverlays`) it
  toggles every sub-owner.
- Layers (`Layer : Fadeable`): `...Layers.<Group>.<Layer>.{Enabled, Fade, Opacity}` plus
  `Settings.Opacity` etc. `BlendTileLevels` is a property of the layer *group*
  (`layergroup.cpp`), e.g. `...Layers.HeightLayers.BlendTileLevels`.
- ScreenSpace renderables (Fadeable): `Enabled, Fade, Opacity, Scale, Rotation,
  UseRadiusAzimuthElevation, RadiusAzimuthElevation`; ScreenSpaceVideo adds a
  `VideoPlayer` sub-owner with trigger props `Play, Pause, GoToStart` and bools
  `PlayAudio, LoopVideo`.
- Slide decks (`data/assets/util/slide_deck_helper.asset`): each slide's ScreenSpace
  identifier is `<deckPrefix><index>` starting at 1 (`deck00_..._energy_I1`).
- Dashboard items: `Dashboard.{Date, Distance, Framerate, GlobeLocation,
  ParallelConnection, SimulationIncrement, CameraVelocity}.*` (identifiers from
  `data/assets/dashboard/*.asset`).
- `Modules.CefWebGui.Reload` (trigger) reloads the in-app CEF GUI.
- `openspace.sessionRecording.startPlayback(file, loop=false, shouldWaitForTiles=true,
  screenshotFps?)` — errors if the file doesn't exist.

## 9. Hosting panels over HTTP (this branch)

The single WebGui static server hosts the panels as an extra endpoint. This branch adds a
`Directories` key to the module's `openspace.cfg` configuration (endpoint/directory
pairs) that seeds `Modules.WebGui.Directories`; `util/webgui.asset` later *appends* its
own endpoints (gui, webpanels, showcomposer, maps) to that property, so config-seeded
entries survive:

```lua
ModuleConfigurations = {
  WebGui = {
    Address = "localhost",
    HttpPort = 4690,                          -- the ONE http port, GUI included
    WebSocketInterface = "DefaultWebSocketInterface",
    Directories = { "panels", "${DATA}" }     -- endpoint/directory pairs
  }
}
```

- Panel URL: `http://localhost:4690/panels/` → `data/index.html` redirects to
  `de_energy_0.21.html` (the Express-based backend answers "Cannot GET /<ep>/" for a
  directory without an `index.html`, so keep that file). The whole `${DATA}` dir is
  served, so the panel's relative `openspace-api.js` / `main2.css` includes resolve.
- The WebGui frontend lives on the same port at `/gui` (and `/` redirects there via the
  `DefaultEndpoint` that `webgui.asset` sets); the in-app CEF GUI follows
  `WebGuiModule::port()` automatically.
- The server is the Node `backend.js` that `data/assets/util/static_server.asset` syncs
  and assigns to `Modules.WebGui.ServerProcessEntryPoint` — nothing is served until that
  asset initializes. No entry point (profile without the webgui asset) → no server.
- The served page still opens its WebSocket to port 4682 directly; the HTTP origin does
  not matter for `ws://` connections.
- Panel-authoring gotcha: `<img>` labels cannot use `file:///` (filesystem) paths when
  the page is served over HTTP — browsers block local resources from an http origin. The
  panel's `mapButtons` emits page-relative URLs for labels when `location.protocol` is
  http, and filesystem paths inside the scripts sent to OpenSpace.

## 10. Debugging workflow

- Server-side symptoms land in `logs/log.html` (rotated: `log-1/2/3.html`) — grep the
  `log-message` cells. `(E) property_setValue ... Property with URI '...' was not found`
  means a stale URI; `Unsupported API version` means an old client (§2).
- Client-side symptoms (nonexistent JS function like `getPropertyValue`, `[1]`-indexing)
  never reach OpenSpace — check the **browser devtools console** instead.
- Two simultaneous WebSocket clients are normal: OpenSpace's embedded CEF GUI (UA
  Chrome/127) plus the external browser — don't confuse their log lines.
- `user/` is in `.gitignore`, so repo-wide file search tools that respect ignore files
  (Glob etc.) will silently miss files there — use `ls`/`find` when verifying data files.
