#!/usr/bin/env python3
"""Run the WebCGM static test suite's official checkpoints against the converter.

The WebCGM 1.0 static test suite (the `static10` module, 232 cases) ships its
methodology in machine-readable form: `static10/webCGMsuite.xml` gives, for
every case, the CGM category and element under test, a purpose, and the
numbered operator checkpoints that constitute the pass criteria.

Those checkpoints are written for a human operator viewing the metafile beside
a reference image. Two kinds are mechanically decidable and are evaluated here:

    "Interpret file X.CGM"          the converter must consume the file
    "...identification string ..."  the test's identifier must appear in the
                                    converted output

The rest ("Verify that a 4-cell CELL ARRAY appears...") require judgement and
are emitted to an operator worksheet instead, alongside the reference image and
this converter's rendering, so a person can adjudicate them.

Nothing here produces a conformance certification. WebCGM conformance is
defined for viewers; a converter can be measured against the rendering
checkpoints, which is what this does.

Usage:
    python scripts/conformance.py --cli build/bin/opencgm_cli
        --suite testdata/webcgm21-ts --out docs/conformance-report.md
        [--worksheet out/worksheet]
        [--rasterizer "resvg -w {w} -h {h} {svg} {png}"]
"""

from __future__ import annotations

import argparse
import collections
import html
import re
import shlex
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

INTERPRET = re.compile(r"^\s*\d+\s+Interpret\s+file\b", re.I)
IDSTRING = re.compile(r"identification\s+string", re.I)
# Some checkpoints name the identifier ("containing CELARY01"), but the suite
# text is not reliable for this: several cases name a different test (PLGSET06
# cites PLGSET05, LINSTL12 cites LINSTL15) and one omits a space
# ("MITRLM01appears"). The checkpoint's intent is that the test's own
# identifier is rendered, so that is what is checked; a mismatch between the
# checkpoint's wording and the case name is reported as a suite anomaly rather
# than charged against the converter.
IDNAME = re.compile(r"containing\s+(?:the\s+)?([A-Z][A-Z0-9_\-]{3,})")
TEXTCONTENT = re.compile(r">([^<>]+)<")

PIPE = "|"
ESCAPED_PIPE = "\\|"


def load_cases(suite: Path):
    xml = suite / "static10" / "webCGMsuite.xml"
    if not xml.is_file():
        raise SystemExit("error: suite description not found: {}".format(xml))
    root = ET.parse(xml).getroot()
    cases = []
    for cat in root.iter("CGMCategory"):
        for tc in cat.iter("TestCase"):
            pts = [c.text.strip() for c in tc.iter("CheckPoint")
                   if c.text and c.text.strip()]
            cases.append({
                "category": cat.get("Name") or "(uncategorised)",
                "name": tc.get("Name") or "",
                "purpose": (tc.findtext("CGMPurpose") or "").strip(),
                "checkpoints": pts,
            })
    return cases


def svg_text(svg: Path) -> str:
    try:
        raw = svg.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""
    return " ".join(TEXTCONTENT.findall(raw))


def evaluate(case, cli: Path, suite: Path, profile: str, workdir: Path):
    static = suite / "static10"
    cgm = None
    for cand in (case["name"] + ".cgm", case["name"] + ".CGM"):
        if (static / cand).is_file():
            cgm = static / cand
            break

    results = []
    if cgm is None:
        for cp in case["checkpoints"]:
            results.append({"text": cp, "kind": "blocked", "verdict": "n/a",
                            "detail": "metafile not present in suite"})
        return results, None

    svg = workdir / (case["name"] + ".svg")
    proc = subprocess.run(
        [str(cli), "--profile", profile, str(cgm), str(svg)],
        capture_output=True)
    converted = proc.returncode == 0 and svg.exists()

    detail = ""
    if not converted:
        err = (proc.stderr or b"").decode("utf-8", "replace").strip().splitlines()
        detail = err[-1] if err else "exit {}".format(proc.returncode)

    text = svg_text(svg) if converted else ""

    for cp in case["checkpoints"]:
        if INTERPRET.search(cp):
            results.append({
                "text": cp, "kind": "interpret",
                "verdict": "pass" if converted else "fail",
                "detail": detail,
            })
        elif IDSTRING.search(cp):
            wanted = case["name"]
            if not converted:
                results.append({"text": cp, "kind": "idstring",
                                "verdict": "fail", "detail": "conversion failed"})
            else:
                ok = wanted.lower() in text.lower()
                note = ""
                m = IDNAME.search(cp)
                cited = m.group(1).rstrip("abcdefghijklmnopqrstuvwxyz") if m else ""
                if cited and cited.upper() != wanted.upper():
                    note = ("suite anomaly: checkpoint cites {} but the case "
                            "is {}".format(cited, wanted))
                results.append({
                    "text": cp, "kind": "idstring",
                    "verdict": "pass" if ok else "fail",
                    "detail": note if ok else
                              "'{}' not found in output text".format(wanted),
                    "note": note,
                })
        else:
            results.append({"text": cp, "kind": "visual",
                            "verdict": "operator", "detail": ""})
    return results, (svg if converted else None)


def build_worksheet(dest: Path, suite: Path, rows):
    dest.mkdir(parents=True, exist_ok=True)
    (dest / "render").mkdir(exist_ok=True)
    refs = dest / "reference"
    refs.mkdir(exist_ok=True)

    css = (
        "body{font:14px/1.5 system-ui,sans-serif;margin:2rem;max-width:1100px}"
        "h2{margin-top:2.5rem;border-bottom:2px solid #ccc;padding-bottom:.3rem}"
        ".case{border:1px solid #ddd;border-radius:6px;padding:1rem;margin:1rem 0}"
        ".imgs{display:flex;gap:1rem;flex-wrap:wrap}"
        ".imgs figure{margin:0}"
        ".imgs img{max-width:420px;border:1px solid #bbb;background:#fff}"
        "figcaption{font-size:12px;color:#555}"
        "li.auto-pass{color:#137333}"
        "li.auto-fail{color:#c5221f;font-weight:600}"
        ".purpose{color:#444;font-style:italic}"
    )

    parts = ["<!doctype html><html><head><meta charset='utf-8'>",
             "<title>WebCGM static10 operator worksheet</title>",
             "<style>" + css + "</style></head><body>",
             "<h1>WebCGM static10 &mdash; operator worksheet</h1>",
             "<p>Checkpoints marked <b>operator</b> need visual adjudication "
             "against the reference image. Mechanically decided checkpoints "
             "are green (pass) or red (fail).</p>"]

    by_cat = collections.defaultdict(list)
    for r in rows:
        by_cat[r["case"]["category"]].append(r)

    marks = {"pass": "&#10003; auto", "fail": "&#10007; auto",
             "operator": "&#9744; operator", "n/a": "&mdash; n/a"}

    for cat in sorted(by_cat):
        parts.append("<h2>" + html.escape(cat) + "</h2>")
        for r in by_cat[cat]:
            case = r["case"]
            parts.append("<div class='case'><h3>"
                         + html.escape(case["name"]) + "</h3>")
            if case["purpose"]:
                parts.append("<p class='purpose'>"
                             + html.escape(case["purpose"]) + "</p>")

            imgs = []
            ref_src = suite / "static10" / "images" / (case["name"] + ".png")
            if ref_src.is_file():
                shutil.copyfile(ref_src, refs / ref_src.name)
                imgs.append(("reference/" + ref_src.name, "reference"))
            png = dest / "render" / (case["name"] + ".png")
            if png.is_file():
                imgs.append(("render/" + png.name, "opencgm"))
            if imgs:
                parts.append("<div class='imgs'>")
                for src, cap in imgs:
                    parts.append("<figure><img src='" + src + "' alt='" + cap
                                 + "'><figcaption>" + cap
                                 + "</figcaption></figure>")
                parts.append("</div>")

            parts.append("<ol>")
            for res in r["results"]:
                cls = {"pass": "auto-pass", "fail": "auto-fail"}.get(
                    res["verdict"], "operator")
                extra = ""
                if res["detail"]:
                    extra = " <em>" + html.escape(res["detail"]) + "</em>"
                parts.append("<li class='" + cls + "'>["
                             + marks[res["verdict"]] + "] "
                             + html.escape(res["text"]) + extra + "</li>")
            parts.append("</ol></div>")

    parts.append("</body></html>")
    (dest / "index.html").write_text("\n".join(parts), encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cli", type=Path, required=True)
    ap.add_argument("--suite", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--profile", default="webcgm")
    ap.add_argument("--worksheet", type=Path)
    ap.add_argument("--rasterizer",
                    help="command template using {svg} {png} {w} {h}; "
                         "renders this converter's output into the worksheet")
    args = ap.parse_args()

    if not args.cli.exists():
        raise SystemExit("error: converter not found: {}".format(args.cli))

    cases = load_cases(args.suite)
    print("{} static10 test cases".format(len(cases)))

    workdir = (args.worksheet / "svg") if args.worksheet \
        else Path("./.conformance-tmp")
    workdir.mkdir(parents=True, exist_ok=True)

    rows = []
    for i, case in enumerate(cases, 1):
        results, svg = evaluate(case, args.cli, args.suite, args.profile, workdir)
        rows.append({"case": case, "results": results, "svg": svg})

        if args.worksheet and args.rasterizer and svg is not None:
            ref = args.suite / "static10" / "images" / (case["name"] + ".png")
            w = h = 500
            if ref.is_file():
                try:
                    from PIL import Image
                    with Image.open(ref) as im:
                        w, h = im.size
                except Exception:
                    pass
            png = args.worksheet / "render" / (case["name"] + ".png")
            png.parent.mkdir(parents=True, exist_ok=True)
            cmd = [tok.format(svg=str(svg), png=str(png), w=w, h=h)
                   for tok in shlex.split(args.rasterizer)]
            subprocess.run(cmd, capture_output=True)

        if i % 40 == 0:
            print("  {}/{}".format(i, len(cases)))

    tally = collections.Counter()
    per_cat = collections.defaultdict(collections.Counter)
    failures = []
    for r in rows:
        cat = r["case"]["category"]
        for res in r["results"]:
            tally[res["verdict"]] += 1
            per_cat[cat][res["verdict"]] += 1
            if res["verdict"] == "fail":
                failures.append((cat, r["case"]["name"], res))

    total = sum(tally.values())
    passed, failed = tally["pass"], tally["fail"]
    operator = tally["operator"]
    auto = passed + failed

    def pct(n):
        return 100.0 * n / total if total else 0.0

    lines = [
        "# WebCGM static10 conformance checkpoints",
        "",
        "Generated by `scripts/conformance.py` from the suite's own methodology",
        "description, `static10/webCGMsuite.xml`, which enumerates the official",
        "operator checkpoints for each WebCGM 1.0 static test case.",
        "",
        "**Scope.** WebCGM conformance is defined for *viewers*. The 20tests and",
        "21tests modules almost entirely exercise a viewer's DOM and XCF APIs",
        "(`getWebCGMDocument`, `getAppStructureById`), and dynamic10 tests link",
        "navigation; none of that applies to a converter. The static10 module is",
        "the part that tests rendering, so it is what is measured here. This is",
        "not a conformance certification.",
        "",
        "**Method.** Checkpoints of the form *Interpret file X* and *verify the",
        "test case identification string appears* are decided mechanically. The",
        "rest describe what a person should see and are routed to an operator",
        "worksheet (`--worksheet`) pairing each reference image with this",
        "converter's rendering.",
        "",
        "Profile: `" + args.profile + "`. Test cases: **" + str(len(cases)) + "**.",
        "",
        "| | Checkpoints |",
        "| --- | --- |",
        "| total | {} |".format(total),
        "| decided mechanically | {} ({:.0f}%) |".format(auto, pct(auto)),
        "| &nbsp;&nbsp;passed | **{}** |".format(passed),
        "| &nbsp;&nbsp;failed | **{}** |".format(failed),
        "| require an operator | {} ({:.0f}%) |".format(operator, pct(operator)),
        "",
        "## By CGM category",
        "",
        "| Category | Cases | Auto pass | Auto fail | Operator |",
        "| --- | --- | --- | --- | --- |",
    ]

    case_counts = collections.Counter(r["case"]["category"] for r in rows)
    for cat in sorted(per_cat):
        c = per_cat[cat]
        lines.append("| {} | {} | {} | {} | {} |".format(
            cat, case_counts[cat], c["pass"], c["fail"], c["operator"]))
    lines.append("")

    anomalies = [(r["case"]["category"], r["case"]["name"], res["note"])
                 for r in rows for res in r["results"] if res.get("note")]
    if anomalies:
        lines += [
            "## Test suite anomalies",
            "",
            "Cases where the suite's own checkpoint text is inconsistent. These",
            "are defects in the published suite, not in the converter, and are",
            "not counted as failures.",
            "",
            "| Category | Test | Note |",
            "| --- | --- | --- |",
        ]
        for cat, name, note in anomalies:
            lines.append("| {} | `{}` | {} |".format(cat, name, note))
        lines.append("")

    lines += ["## Failed checkpoints", ""]
    if failures:
        lines += ["| Category | Test | Checkpoint | Detail |",
                  "| --- | --- | --- | --- |"]
        for cat, name, res in failures:
            txt = res["text"].replace(PIPE, ESCAPED_PIPE)[:110]
            det = res["detail"].replace(PIPE, ESCAPED_PIPE)[:70]
            lines.append("| {} | `{}` | {} | {} |".format(cat, name, txt, det))
    else:
        lines.append("None.")
    lines.append("")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(lines), encoding="utf-8")
    print("mechanical: {} passed, {} failed; {} need an operator".format(
        passed, failed, operator))
    print("wrote {}".format(args.out))

    if args.worksheet:
        build_worksheet(args.worksheet, args.suite, rows)
        print("wrote {}".format(args.worksheet / "index.html"))

    # Non-zero exit on any mechanically decided failure, so this can gate CI.
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
