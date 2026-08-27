#!/usr/bin/env python3
"""Screen converted output against a corpus's reference images.

For every CGM that ships a same-named reference PNG, this converts the CGM to
SVG, rasterises that SVG at the reference's dimensions, and measures how far
the two images diverge.

This is a regression *screen*, not a conformance result. Rasterising SVG
requires font choices that will not match whatever renderer produced the
reference images, so text-heavy cases diverge even when the conversion is
correct. Comparison is therefore done on heavily downsampled, blurred
greyscale, which suppresses glyph and antialiasing differences while still
catching missing geometry, wrong placement and wrong colour. Treat the output
as a ranked list of cases worth looking at by eye.

An external rasteriser is required. Pass a command template using the
placeholders {svg} {png} {w} {h}, for example:

    --rasterizer "resvg --width {w} --height {h} {svg} {png}"
    --rasterizer "rsvg-convert -w {w} -h {h} -o {png} {svg}"
"""

from __future__ import annotations

import argparse
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

GRID = 64          # comparison resolution; small enough to ignore glyph detail
BLUR = 1.2
INK_THRESHOLD = 0.9  # below this (0..1 luminance) a pixel counts as "ink"


# The dynamic tests exercise interactive behaviour -- link navigation, picture
# replacement, blanking. Their reference images capture the state *after* that
# interaction, and are often the reference for the link's target file rather
# than for the same-named source. Pairing them by filename compares unrelated
# drawings, so they are excluded by default.
DEFAULT_EXCLUDE = ("dynamic10",)


def pairs(corpus: Path, exclude: tuple[str, ...]) -> list[tuple[Path, Path]]:
    found = []
    for sub in sorted(p for p in corpus.iterdir() if p.is_dir()):
        if sub.name in exclude:
            continue
        pngs = {p.stem.lower(): p for p in sub.glob("*.png")}
        for pat in ("*.cgm", "*.CGM"):
            for cgm in sub.glob(pat):
                ref = pngs.get(cgm.stem.lower())
                if ref is not None:
                    found.append((cgm, ref))
    return sorted(set(found))


def signature(path: Path) -> tuple[np.ndarray, float]:
    """Downsampled, blurred greyscale plus the fraction of inked pixels."""
    with Image.open(path) as im:
        grey = im.convert("L")
        full = np.asarray(grey, dtype=np.float32) / 255.0
        ink = float((full < INK_THRESHOLD).mean())
        small = grey.resize((GRID, GRID), Image.LANCZOS).filter(
            ImageFilter.GaussianBlur(BLUR))
        arr = np.asarray(small, dtype=np.float32) / 255.0
    return arr, ink


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cli", type=Path, required=True)
    ap.add_argument("--corpus", type=Path, required=True)
    ap.add_argument("--rasterizer", required=True,
                    help="command template using {svg} {png} {w} {h}")
    ap.add_argument("--profile", default="webcgm")
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--flag-above", type=float, default=0.15,
                    help="dissimilarity above which a case is flagged")
    ap.add_argument("--exclude-groups", nargs="*", default=list(DEFAULT_EXCLUDE),
                    help="corpus subdirectories to skip "
                         f"(default: {' '.join(DEFAULT_EXCLUDE)})")
    args = ap.parse_args()

    todo = pairs(args.corpus, tuple(args.exclude_groups))
    if not todo:
        print(f"error: no CGM/PNG pairs under {args.corpus}", file=sys.stderr)
        return 2
    print(f"{len(todo)} reference-paired files")

    rows, skipped = [], []
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        for i, (cgm, ref) in enumerate(todo, 1):
            svg, png = tmp / f"{i}.svg", tmp / f"{i}.png"

            conv = subprocess.run(
                [str(args.cli), "--profile", args.profile, str(cgm), str(svg)],
                capture_output=True)
            if conv.returncode != 0 or not svg.exists():
                skipped.append((cgm, "conversion failed"))
                continue

            with Image.open(ref) as im:
                w, h = im.size
            # Split the template first, then substitute, so that backslashes
            # in Windows paths are never treated as shell escapes.
            cmd = [tok.format(svg=str(svg), png=str(png), w=w, h=h)
                   for tok in shlex.split(args.rasterizer)]
            rast = subprocess.run(cmd, capture_output=True)
            if rast.returncode != 0 or not png.exists():
                detail = (rast.stderr or b"").decode("utf-8", "replace").strip()
                skipped.append((cgm, f"rasterise failed: {detail[:80]}"))
                continue

            a, ink_out = signature(png)
            b, ink_ref = signature(ref)
            rmse = float(np.sqrt(np.mean((a - b) ** 2)))
            rows.append({
                "group": cgm.parent.name, "name": cgm.stem,
                "dissim": rmse, "ink_out": ink_out, "ink_ref": ink_ref,
                "ink_delta": abs(ink_out - ink_ref),
            })
            for f in (svg, png):
                if f.exists():
                    f.unlink()
            if i % 20 == 0:
                print(f"  {i}/{len(todo)}")

    rows.sort(key=lambda r: -r["dissim"])
    flagged = [r for r in rows if r["dissim"] > args.flag_above]
    d = np.array([r["dissim"] for r in rows]) if rows else np.array([0.0])

    lines = [
        "# Visual comparison against reference images",
        "",
        "Generated by `scripts/visual-compare.py`.",
        "",
        "**This is a regression screen, not a conformance result.** Rasterising",
        "SVG requires font choices that do not match whatever renderer produced",
        "the reference images, so text-heavy cases diverge even when the",
        "conversion is correct. Images are compared as "
        f"{GRID}x{GRID} blurred greyscale to suppress glyph and antialiasing",
        "differences. Use this as a ranked list of cases to inspect by eye.",
        "",
        f"Profile: `{args.profile}`. Compared: **{len(rows)}** of {len(todo)} "
        f"reference-paired files.",
        "",
        f"Excluded groups: {', '.join(f'`{g}`' for g in args.exclude_groups) or 'none'}."
        " The dynamic tests exercise link navigation and picture"
        " replacement; their reference images show the state after that"
        " interaction, and frequently belong to the link's target file rather"
        " than the same-named source, so filename pairing compares unrelated"
        " drawings.",
        "",
        "| Statistic | Dissimilarity (0 = identical) |",
        "| --- | --- |",
        f"| median | {np.median(d):.4f} |",
        f"| mean | {d.mean():.4f} |",
        f"| 90th percentile | {np.percentile(d, 90):.4f} |",
        f"| max | {d.max():.4f} |",
        f"| flagged (> {args.flag_above}) | {len(flagged)} of {len(rows)} |",
        "",
    ]

    if flagged:
        lines += [
            "## Flagged for inspection",
            "",
            "`ink` is the fraction of non-white pixels. A large ink difference",
            "suggests missing or extra geometry; similar ink with high",
            "dissimilarity is more often font or placement.",
            "",
            "| Group | Test | Dissimilarity | ink out | ink ref |",
            "| --- | --- | --- | --- | --- |",
        ]
        for r in flagged:
            lines.append(
                f"| `{r['group']}` | `{r['name']}` | {r['dissim']:.4f} | "
                f"{r['ink_out']:.3f} | {r['ink_ref']:.3f} |")
        lines.append("")

    lines += ["## Closest matches", "",
              "| Group | Test | Dissimilarity |", "| --- | --- | --- |"]
    for r in rows[-10:][::-1]:
        lines.append(f"| `{r['group']}` | `{r['name']}` | {r['dissim']:.4f} |")
    lines.append("")

    if skipped:
        lines += ["## Not compared", "", "| File | Reason |", "| --- | --- |"]
        for path, why in skipped:
            lines.append(f"| `{path.name}` | {why} |")
        lines.append("")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(lines), encoding="utf-8")
    print(f"compared {len(rows)}, flagged {len(flagged)}, skipped {len(skipped)}")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
