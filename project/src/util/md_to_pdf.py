#!/usr/bin/env python3
"""Convert a Markdown file to PDF (e.g. REPORT.md -> REPORT.pdf).

Pure-Python pipeline (no pandoc/LaTeX needed): markdown -> HTML -> PDF via
python-markdown + WeasyPrint. Relative images (like the embedded speedup.png) resolve
against the Markdown file's directory, and tables are rendered with light styling.
"""

import argparse
import sys
from pathlib import Path

import markdown
from weasyprint import HTML

SCRIPT_DIR = Path(__file__).resolve().parent

# Minimal print stylesheet: readable body, bordered tables, fitted images.
CSS = """
@page { size: A4; margin: 18mm; }
body { font-family: sans-serif; font-size: 11pt; line-height: 1.45; color: #1a1a1a; }
h1 { font-size: 20pt; } h2 { font-size: 15pt; margin-top: 1.2em; }
h3 { font-size: 12.5pt; }
code { font-family: monospace; background: #f2f2f2; padding: 1px 3px; border-radius: 3px; }
table { border-collapse: collapse; margin: 0.6em 0; }
th, td { border: 1px solid #999; padding: 4px 9px; text-align: right; }
th { background: #efefef; }
img { max-width: 100%; }
"""


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert a Markdown file to PDF.")
    parser.add_argument(
        "input",
        nargs="?",
        default="REPORT.md",
        help="input Markdown file (default: REPORT.md in the cwd)",
    )
    parser.add_argument(
        "output",
        nargs="?",
        default=None,
        help="output PDF file (default: input path with a .pdf suffix)",
    )
    args = parser.parse_args()

    in_path = Path(args.input)
    out_path = Path(args.output) if args.output else in_path.with_suffix(".pdf")

    if not in_path.exists():
        print(f"error: input file not found: {in_path}", file=sys.stderr)
        return 1

    html_body = markdown.markdown(
        in_path.read_text(encoding="utf-8"),
        extensions=["tables", "fenced_code"],
    )
    html_doc = f"<!doctype html><meta charset=utf-8><style>{CSS}</style>{html_body}"

    # base_url = the Markdown file's directory so relative images (speedup.png) load.
    HTML(string=html_doc, base_url=str(in_path.resolve().parent)).write_pdf(str(out_path))

    print(f"{in_path.name} -> {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
