# /// script
# requires-python = ">=3.10"
# dependencies = []
# ///
"""Convert a video file into a Basis Universal ETC1S texture video (.ktx2) for use with
OpenSpace's RegionalVideoTileProvider.

Pipeline: ffprobe (metadata) -> ffmpeg (PNG frames, scaled/padded to multiples of 4,
vertically flipped for OpenGL) -> basisu (ETC1S KTX2 with mipmaps and animation
metadata).

Requires ffmpeg/ffprobe on PATH and the basisu encoder tool (build it from
modules/video/ext/basis_universal with CMake, or pass --basisu).

Example:
    uv run convert_video.py input.mp4 -o overlay.ktx2 --fps 12 --max-size 2048
"""

from __future__ import annotations

import argparse
import json
import math
import shutil
import subprocess
import sys
import tempfile
from fractions import Fraction
from pathlib import Path


def run(cmd: list[str], **kwargs) -> subprocess.CompletedProcess:
    print("+", " ".join(str(c) for c in cmd))
    return subprocess.run(cmd, check=True, **kwargs)


def probe(ffprobe: str, video: Path) -> tuple[int, int, float]:
    """Return (width, height, fps) of the first video stream."""
    result = run(
        [
            ffprobe, "-v", "error", "-select_streams", "v:0",
            "-show_entries", "stream=width,height,avg_frame_rate",
            "-of", "json", str(video),
        ],
        capture_output=True,
        text=True,
    )
    stream = json.loads(result.stdout)["streams"][0]
    fps = float(Fraction(stream["avg_frame_rate"]))
    return int(stream["width"]), int(stream["height"]), fps


def target_size(width: int, height: int, max_size: int) -> tuple[int, int]:
    """Scale to fit max_size, then round down to multiples of 4 (BC block size)."""
    scale = min(1.0, max_size / max(width, height))
    w = max(4, int(width * scale) // 4 * 4)
    h = max(4, int(height * scale) // 4 * 4)
    return w, h


def fps_to_animdata(fps: float) -> tuple[int, int]:
    """KTXanimData: each frame lasts `duration` ticks of a `timescale` ticks/s clock."""
    frac = Fraction(fps).limit_denominator(10000)
    return frac.denominator, frac.numerator  # duration, timescale


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("video", type=Path, help="Input video file (anything ffmpeg reads)")
    parser.add_argument("-o", "--output", type=Path, help="Output .ktx2 path")
    parser.add_argument("--fps", type=float, help="Resample to this frame rate (default: source rate)")
    parser.add_argument("--max-size", type=int, default=2048,
                        help="Maximum output dimension; frames are scaled down to fit (default 2048)")
    parser.add_argument("--quality", type=int, default=192,
                        help="ETC1S quality 1-255 (default 192)")
    parser.add_argument("--comp-level", type=int, default=2,
                        help="Encoder effort 0-6; 2+ recommended for video (default 2)")
    parser.add_argument("--random-access", action="store_true",
                        help="Encode as a 2D array (every frame an I-frame) instead of "
                             "conditional-replenishment video. Larger file, but backwards "
                             "scrubbing does not re-decode from the start")
    parser.add_argument("--no-flip", dest="flip", action="store_false",
                        help="Do not flip frames vertically. The default flip matches "
                             "OpenSpace's bottom-up texture orientation; if the overlay "
                             "renders upside down, toggle this")
    parser.add_argument("--no-mipmaps", dest="mipmaps", action="store_false",
                        help="Do not generate mipmaps (not recommended; causes aliasing "
                             "when zoomed out)")
    parser.add_argument("--basisu", type=Path, help="Path to the basisu encoder tool")
    parser.add_argument("--keep-frames", action="store_true",
                        help="Keep the temporary PNG frame directory")
    args = parser.parse_args()

    ffmpeg = shutil.which("ffmpeg")
    ffprobe = shutil.which("ffprobe")
    if not ffmpeg or not ffprobe:
        print("error: ffmpeg/ffprobe not found on PATH", file=sys.stderr)
        return 1
    basisu = str(args.basisu) if args.basisu else shutil.which("basisu")
    if not basisu or not Path(basisu).exists():
        print(
            "error: basisu encoder not found. Build it from "
            "modules/video/ext/basis_universal (cmake -B build && cmake --build build "
            "--target basisu) or pass --basisu <path>",
            file=sys.stderr,
        )
        return 1
    if not args.video.exists():
        print(f"error: input '{args.video}' does not exist", file=sys.stderr)
        return 1

    output = args.output or args.video.with_suffix(".ktx2")
    src_w, src_h, src_fps = probe(ffprobe, args.video)
    fps = args.fps or src_fps
    w, h = target_size(src_w, src_h, args.max_size)
    print(f"source {src_w}x{src_h} @ {src_fps:g} fps -> {w}x{h} @ {fps:g} fps")

    frame_dir = Path(tempfile.mkdtemp(prefix="texturevideo_"))
    try:
        filters = [f"fps={fps:g}", f"scale={w}:{h}:flags=lanczos"]
        if args.flip:
            filters.append("vflip")
        run([
            ffmpeg, "-y", "-loglevel", "error", "-i", str(args.video),
            "-vf", ",".join(filters),
            str(frame_dir / "frame_%06d.png"),
        ])
        n_frames = len(list(frame_dir.glob("frame_*.png")))
        if n_frames == 0:
            print("error: ffmpeg produced no frames", file=sys.stderr)
            return 1
        print(f"{n_frames} frames extracted")

        duration, timescale = fps_to_animdata(fps)
        cmd = [
            basisu,
            "-ktx2",
            "-tex_type", "2darray" if args.random_access else "video",
            "-q", str(args.quality),
            "-comp_level", str(args.comp_level),
            "-ktx2_animdata_duration", str(duration),
            "-ktx2_animdata_timescale", str(timescale),
            "-output_file", str(output.resolve()),
            "-multifile_printf", "frame_%06u.png",
            "-multifile_first", "1",
            "-multifile_num", str(n_frames),
        ]
        if args.mipmaps:
            cmd.append("-mipmap")
        run(cmd, cwd=frame_dir)
    finally:
        if args.keep_frames:
            print(f"frames kept in {frame_dir}")
        else:
            shutil.rmtree(frame_dir, ignore_errors=True)

    size_mib = output.stat().st_size / (1024 * 1024)
    bpp = output.stat().st_size * 8 / (w * h * n_frames)
    print(f"wrote {output} ({size_mib:.1f} MiB, {bpp:.2f} bits/texel)")
    print("\nAsset snippet:")
    print(f'''  {{
    Identifier = "MyRegionalVideo",
    Type = "RegionalVideoTileProvider",
    Video = "{output.name}",
    Extent = {{ MinLon = ..., MinLat = ..., MaxLon = ..., MaxLat = ... }},
    StartTime = "YYYY MM DD HH:MM:SS",
    EndTime = "YYYY MM DD HH:MM:SS",
    PlaybackMode = "MapToSimulationTime"
  }}''')
    return 0


if __name__ == "__main__":
    sys.exit(main())
