# Digital Earth "Energy I" — repo layout, media policy, and Git constraints

Documents the 2026-07-01 relocation of the show content out of the gitignored `user/`
tree into the repo (commit `5bac969e8d`), the decisions behind it, and the Git/GitHub
constraints that shaped them.

## 1. Where everything lives now

| Content | Old (gitignored) | New (tracked) |
|---|---|---|
| Show assets + data | `user/data/assets/digital_earth/ENERGY_I/` | `data/assets/digital_earth/ENERGY_I/` |
| Profiles (`de_energy_0.21`, `custom_test`, `smaller`) | `user/data/profiles/` | `data/profiles/` |
| Session recording (`01.North.America.START.osrectxt`) | `user/recordings/` | `data/recordings/` |
| Web control panel (`de_energy_0.21.html`, `openspace-api.js`, `main2.css`) | `user/` | `data/` |

**Open the panel from `data/de_energy_0.21.html`** (plain `file://` in a browser while
OpenSpace runs; see `notes/openspace-web-api.md` for the connection protocol).

Still in `user/` on purpose (machine-local runtime state, still gitignored):
`screenshots/`, `config/` (e.g. `two-viewports.json`), `showcomposer/`, `globebrowsing/`
cache, `webpanels/` (empty), `user/data/assets/run.out`.

## 2. Path-token rewrites that made the move work

- `${USER_ASSETS}/digital_earth/...` → `${ASSETS}/digital_earth/...` in the panel
  (22 refs), the ENERGY_I assets (210 refs), and the profiles' asset lists.
- `ROOT/user/data/assets/...` → `ROOT/data/assets/...` in the panel (93 refs — button
  icons, GDAL `addLayer` FilePaths, audio mp3 paths). `ROOT/` is a panel-side convention:
  `mapButtons` string-replaces it with `absPath("${BASE}") + "/"` at connect time.
- `${RECORDINGS}/01...` → `${DATA}/recordings/01...` (the `RECORDINGS` token still points
  to `${USER}/recordings`; we bypassed it rather than editing `openspace.cfg`).
- Profiles resolve bare asset names against both `${ASSETS}` and `${USER_ASSETS}`, so only
  entries with an explicit `${USER_ASSETS}/` prefix needed rewriting.
- 10 `US_EIA_OPERATING_GENERATORS/*.asset` files had hardcoded absolute paths from an old
  install (`C:/OpenSpace/OpenSpace-0.21.2/user/...`) — replaced with
  `asset.resource("<file>.vrt")`. Rule of thumb: **any file referenced by an asset should
  go through `asset.resource()` (relative to the asset) or a path token — never a bare
  relative string** (fails codegen's file-existence verifier by resolving against CWD)
  **and never an absolute path** (dies on the next machine).

## 3. Media policy: mp4/mp3 are local-only (NOT in git)

`.gitignore` (repo root) ignores `data/assets/digital_earth/**/*.mp4` and `**/*.mp3`
(~920 MB). They stay on disk where assets/panel reference them, but are not tracked.

Why, in order of discovery:
1. **GitHub hard-rejects raw files > 100 MB.** Four show mp4s are 138–236 MB, so raw
   commit + push is impossible for them.
2. **Git LFS was tried first and backed out**: `git lfs push` failed with
   *"This repository exceeded its LFS budget"* — the account's LFS storage/bandwidth
   quota was already exhausted. The LFS `.gitattributes` rules were removed and the
   commit amended before anything reached the remote.
3. GitHub *warns* (but accepts) raw files > 50 MB — the 71 MB
   `transmission_100-161kV.geojson` is committed raw and just triggers the warning.

**To restore LFS later** (after buying a data pack / freeing quota):
```bash
git lfs track "data/assets/digital_earth/**/*.mp4" "data/assets/digital_earth/**/*.mp3"
# remove the two ignore lines from .gitignore, then:
git add .gitattributes .gitignore data/assets/digital_earth
git commit && git push   # LFS objects upload on push
```
Caveat: `git lfs track` rewrites `.gitattributes` and strips its blank lines — append the
filter lines manually instead to keep the diff clean.

**Consequence for fresh clones**: the media is not in the repo. A new machine needs the
mp4/mp3 files copied in out-of-band (or the LFS restore above done first) or the show's
video/audio pieces will fail to load — everything else works.

## 4. Misc facts worth remembering

- `user/` is in `.gitignore`, so ignore-respecting search tools (Glob etc.) silently skip
  it — this caused a real misdiagnosis (a "missing" geojson that existed all along). Use
  `ls`/`find` when checking data files under ignored trees.
- `git lfs status` / `git show :<path>` distinguish a staged LFS pointer (~134 bytes,
  `version https://git-lfs.github.com/spec/v1`) from a staged raw blob — check this
  before pushing, not after.
- The dirty `modules/molecule/ext/mold` submodule is pre-existing local state; keep it out
  of commits.
- `data/profiles/smaller.profile` references `${USER_ASSETS}/base_smaller`, which doesn't
  exist anywhere — pre-existing dangling reference, left as-is.
- CRLF: the moved CSV/asset files are CRLF on disk; `* text=auto` in `.gitattributes`
  normalizes them to LF in the repo (the mass `git add` warnings were expected, benign).

## 5. Related notes

- `notes/openspace-web-api.md` — WebSocket API protocol, client library pitfalls,
  property-URI reference, debugging workflow for the control panel.
- `notes/globebrowsing-geojson.md` — GeoJSON layer architecture + field notes
  (`asset.resource` requirement, `makeIdentifier` punctuation mapping).
- `CHANGELOG.md` — session-by-session branch history.
