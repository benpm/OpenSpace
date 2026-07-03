# Plan: port `data/de_energy_0.21.html` to OpenSpace-native in-app panels

Drafted 2026-07-03. Status: **implemented 2026-07-03** — all 164 buttons ported to
`data/assets/digital_earth/ENERGY_I/actions/*.asset` (14 section assets + master),
`mission.asset` added, profile updated. Verified live: all actions registered, mission
timeline renders with era phases and icon imagery (served over the panels HTTP endpoint,
since the GUI loads mission images as URLs), zero errors on a clean profile load.
Deviations from the plan noted inline below; two broken JS-syntax actions found in the
profile were also fixed along the way.

## The native mechanisms (verified in source + docs)

OpenSpace has two first-class, in-app UI systems that together replace the HTML dashboard:

1. **Actions Panel** — every action registered via `openspace.action.registerAction`
   appears as a clickable button in the in-app GUI, organized into folders by `GuiPath`.
   This is the canonical "button dashboard" mechanism (used by e.g.
   `data/assets/scene/solarsystem/missions/apollo/11/actions.asset`).
   Action fields (`include/openspace/interaction/action.h`): `Identifier`, `Command`
   (Lua), `Name`, `Documentation`, `GuiPath`, optional `Color`/`TextColor` (vec4, UI
   grouping hints), `IsLocal` (No = synced to cluster/parallel peers — important for
   dome shows).

2. **Missions Panel** — `openspace.loadMission(dict)` (`src/mission/mission.cpp`,
   `missionmanager_lua.inl`) drives a narrative side panel with a vertical timeline:
   nested **Phases**, each with `Name`, `Description`, `Image`, `Link`,
   **`Actions` (list of registered action identifiers, shown as buttons in the phase)**,
   `TimeRange`, and dated `Milestones`. Clicking timeline entries jumps time and sets
   the scene. This is the "mission planner" thing.

The show's slides already are native (`ScreenSpaceImageLocal` decks via
`util/slide_deck_helper.asset`) — only the *controls* live in HTML today.

## Why the port is nearly mechanical

The panel's buttons are JS async functions whose bodies are 95% `openspace.*` calls —
the *same* API exists in Lua. Porting a button = strip `await`/JS syntax, keep the calls:

```js
// HTML button                                   // Native action Command
await openspace.globebrowsing.addGeoJsonFromFile(  openspace.globebrowsing.addGeoJsonFromFile(
  '${ASSETS}/digital_earth/.../ERCOT.geojson');      "${ASSETS}/digital_earth/.../ERCOT.geojson")
await openspace.setPropertyValueSingle(...)        openspace.setPropertyValueSingle(...)
```

Special cases, all solvable natively:
- **State-dependent toggles** (SHADING/blackout/sun-glare read a value, then branch):
  Lua reads `openspace.propertyValue(uri)` directly inside the Command — simpler than
  the JS round-trip. (`openspace.toggleFade` already handles most toggle buttons.)
- **`setTimeout` sequencing** (Movies SETUP, fade-then-disable patterns): use
  `setPropertyValue(uri, value, duration, easing, postscript)` — the `postscript`
  Lua string runs when the interpolation completes. No timers needed.
- **`ROOT/` path injection**: gone — Lua takes `${ASSETS}`/`${DATA}` tokens natively.
- **Audio buttons**: `openspace.audio.*` identical in Lua.
- **Button icons**: the Actions panel doesn't show per-button images; use
  `Color`/`TextColor` for the visual grouping the panel's orange/red/lime labels
  provide, and put the imagery in Mission phase `Image`s instead.

## Structure

New directory `data/assets/digital_earth/ENERGY_I/actions/`:

- One asset per panel card, mirroring the 14 cards:
  `setup.asset`, `songs_sailing.asset`, `fires.asset`, `whaling.asset`,
  `whale_slides.asset`, `coal.asset`, `oil.asset`, `electricity.asset`,
  `high_voltage.asset`, `gdal_layers.asset`, `supply_demand_slides.asset`,
  `gas_pipelines.asset`, `credits.asset`, `songs_oil.asset`.
- Each registers its actions in `asset.onInitialize` / removes them in
  `onDeinitialize` (Apollo pattern), exports the identifiers.
  - Identifiers: `de.energy.<section>.<action>` (e.g. `de.energy.coal.fields_load`).
  - `GuiPath`: `/Digital Earth/<Card Name>` (e.g. `/Digital Earth/4. Coal`) — the
    Actions panel renders these as folders in show order.
  - `IsLocal = false` so a cluster/parallel setup stays in sync (the HTML panel could
    only ever drive the connected instance).
- `actions.asset` master that `asset.require`s all of the above; added to the
  `de_energy_0.21` profile's asset list.
- LOAD-type buttons should become **idempotent** in Lua (guard with
  `openspace.hasProperty("Scene.Earth.Renderable.GeographicOverlays.<id>.Enabled")`
  before `addGeoJsonFromFile`) so double-triggering doesn't error — an improvement over
  the HTML panel.

Mission asset `data/assets/digital_earth/ENERGY_I/mission.asset`:

- One `Mission` dict, `Identifier = "DigitalEarthEnergyI"`, loaded via
  `openspace.loadMission` on initialize / `unloadMission` on deinitialize.
- Phases follow the show's narrative arc, which conveniently *is* temporal:
  1. *Age of Whale Oil* — TimeRange ~1835–1860 (whaling voyages animation era),
     phase Actions = whaling/sailing-songs/slide actions, Image = a whaling slide.
  2. *Age of Coal* — ~1880–present (coal plants 1935–2028 dataset), coal actions.
  3. *Oil & Gas* — pipelines/pano actions.
  4. *Era of Electricity* — generators, transmission, interconnects, GridStatus LMP.
  5. *Supply & Demand / Credits* — slide + credits actions.
- Milestones for the datasets' key dates (first US central station 1882, etc. — content
  team's call). Clicking phases jumps simulation time to the era, which the show
  already manipulates (1835 voyages, 1935–2028 coal animation).
- Phase `Image`s reuse `energy_1_slides/ICONS/*.jpg`.

Keybindings: move the panel's implied hotkeys (CTRL+F12 setup, F5 reload) into the
profile's `keybindings` section bound to the new action identifiers.

## Porting order (each step independently testable)

1. `setup.asset` (DE Basic SETUP, Movies SETUP, slide-position setups) — exercises the
   postscript-sequencing pattern.
2. Layer cards (coal, oil, gas, high-voltage, interconnects, GDAL) — mechanical
   translation, plus idempotency guards.
3. Media cards (songs via `openspace.audio`, video players, slides).
4. Toggle cards (atmosphere/shading/night — the propertyValue-branch pattern).
5. `mission.asset` referencing the registered identifiers.
6. Profile update (add assets, keybindings) + retire the HTML panel to a fallback
   (it keeps working unchanged over the `/panels` server; both can coexist since
   actions and panel scripts drive the same properties).

## Verification

- Launch with the de_energy profile; open the in-app menu → Actions panel: 14 folders
  under `/Digital Earth`, buttons trigger the same log-visible scripts (`ScriptLog.txt`)
  as their HTML counterparts did.
- Missions panel shows the show timeline; clicking a phase jumps time and its action
  buttons work.
- Regression: press each ported action once with `logs/log.html` open — zero error rows
  (same bar as the HTML panel after the GeoJSON parser fix).
- Cluster correctness: `IsLocal = false` actions verified over a parallel connection if
  available.

## Sources
- Actions/Missions panel behavior: [Orientation — OpenSpace docs](https://docs.openspaceproject.com/latest/getting-started/orientation/index.html)
- Action schema: `include/openspace/interaction/action.h`, `src/interaction/actionmanager_lua.inl`
- Mission schema: `src/mission/mission.cpp` (codegen params), `src/mission/missionmanager_lua.inl`,
  example `data/assets/scene/solarsystem/heliosphere/todayssun/mission.asset`
- Action asset pattern: `data/assets/scene/solarsystem/missions/apollo/11/actions.asset`
