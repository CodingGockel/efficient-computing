#!/usr/bin/env python3
"""Convert a PPM image (e.g. the ray tracer's output.ppm) to PNG.

The C++ renderer writes a P3 PPM, which Windows can't open natively. This script
builds a PNG from it so both files are available. Uses Pillow.
"""

import argparse
import sys
from pathlib import Path

from PIL import Image

SCRIPT_DIR = Path(__file__).resolve().parent


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert a PPM image to PNG.")
    parser.add_argument(
        "input",
        nargs="?",
        default=str(SCRIPT_DIR / "output.ppm"),
        help="input PPM file (default: output.ppm next to this script)",
    )
    parser.add_argument(
        "output",
        nargs="?",
        default=None,
        help="output PNG file (default: input path with a .png suffix)",
    )
    args = parser.parse_args()

    in_path = Path(args.input)
    out_path = Path(args.output) if args.output else in_path.with_suffix(".png")

    try:
        with Image.open(in_path) as im:
            im.load()
            im.save(out_path)
            w, h = im.size
    except FileNotFoundError:
        print(f"error: input file not found: {in_path}", file=sys.stderr)
        return 1
    except (OSError, ValueError) as exc:
        print(f"error: could not convert {in_path}: {exc}", file=sys.stderr)
        return 1

    print(f"{in_path.name} {w}x{h} -> {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
