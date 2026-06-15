#!/usr/bin/env python3
"""Convert a binary STL file to ASCII STL.

The C++ ray tracer's load_stl only parses ASCII STL (it scans for `vertex`
lines). Binary STL files (e.g. car.stl) therefore load 0 triangles. This script
converts a binary STL to ASCII so the renderer can read it. Uses numpy-stl,
which auto-detects binary vs. ASCII on read and can save explicitly as ASCII.
"""

import argparse
import sys
from pathlib import Path

from stl import mesh, Mode


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert a binary STL file to ASCII STL.")
    parser.add_argument("input", help="input STL file (binary or ASCII)")
    parser.add_argument(
        "output",
        nargs="?",
        default=None,
        help="output ASCII STL file (default: input stem + _ascii.stl)",
    )
    args = parser.parse_args()

    in_path = Path(args.input)
    out_path = (
        Path(args.output)
        if args.output
        else in_path.with_name(in_path.stem + "_ascii.stl")
    )

    try:
        m = mesh.Mesh.from_file(str(in_path))
    except FileNotFoundError:
        print(f"error: input file not found: {in_path}", file=sys.stderr)
        return 1
    except Exception as exc:  # numpy-stl raises various errors for malformed files
        print(f"error: could not read STL {in_path}: {exc}", file=sys.stderr)
        return 1

    m.save(str(out_path), mode=Mode.ASCII)
    print(f"{in_path.name}: {len(m.vectors)} triangles -> {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
