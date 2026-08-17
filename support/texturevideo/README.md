# Texture video conversion

`RegionalVideoTileProvider` (modules/video) plays *texture videos*: Basis Universal
ETC1S encoded KTX2 files whose array layers are the video frames. Frames are transcoded
at runtime to a GPU block-compressed format (BC7, or BC3 on very old hardware) and
uploaded as compressed textures, so VRAM use and upload bandwidth stay low and no
video-codec dependency is needed at runtime.

## Converting a video

Requirements:
- `ffmpeg` / `ffprobe` on PATH
- the `basisu` encoder tool. Build it from the bundled submodule:
  ```
  cmake -S modules/video/ext/basis_universal -B <builddir>
  cmake --build <builddir> --config Release --target basisu
  ```
  (the executable lands in `modules/video/ext/basis_universal/bin/`), or use a release
  binary and pass `--basisu <path>`.

Convert:

```
uv run support/texturevideo/convert_video.py input.mp4 -o overlay.ktx2 --fps 12
```

Useful options:

| Option | Effect |
|---|---|
| `--fps N` | Resample to N frames/s. Fewer frames = smaller file; simulation-time playback interpolates the frame index anyway, so 10-15 fps is usually plenty. |
| `--max-size N` | Downscale so the longest side is at most N (default 2048). |
| `--quality 1-255` | ETC1S quality/size tradeoff (default 192). |
| `--random-access` | Store every frame as an I-frame (2D array) instead of conditional-replenishment video. The file gets larger, but scrubbing backwards through simulation time no longer re-decodes from frame 0. Use for content that is viewed with heavy time-scrubbing. |
| `--no-flip` | Skip the vertical flip. The default flip matches OpenSpace's bottom-up texture orientation; if your overlay renders upside down, toggle this. |

Notes:
- In the default `video` mode only frame 0 is an I-frame and frames must be decoded in
  order, so backwards playback re-decodes from the start of the file. The runtime
  bounds this with prefetching, but for scrub-heavy datasets prefer `--random-access`.
- Frame dimensions are rounded to multiples of 4 (BC block size); mipmaps are generated
  by default.
- The frame rate is stored in the file (`KTXanimData`); `FramesPerSecond` in the asset
  overrides it.

## Using the result in an asset

```lua
local Layer = {
  Identifier = "US_Energy_Animation",
  Type = "RegionalVideoTileProvider",
  Video = asset.resource("overlay.ktx2"),
  -- Degrees; the extent must not cross the antimeridian
  Extent = { MinLon = -125.0, MinLat = 24.0, MaxLon = -66.5, MaxLat = 49.5 },
  StartTime = "2020 01 01 00:00:00",
  EndTime = "2020 01 02 00:00:00",
  PlaybackMode = "MapToSimulationTime",
  Enabled = true
}

asset.onInitialize(function()
  openspace.globebrowsing.addLayer("Earth", "ColorLayers", Layer)
end)
```

Add the layer to the `ColorLayers` group; its Normal blend respects the transparent
area outside the extent (the `Overlays` group double-blends and is not recommended).
`HideOutsideRange = true` makes the overlay disappear outside `[StartTime, EndTime]`
instead of freezing on the first/last frame.
