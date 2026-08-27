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
import base64
import collections
import functools
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


@functools.lru_cache(maxsize=None)
def metafile_index(static: Path):
    """Map lowercased filename -> real path.

    The suite is inconsistent about case: webCGMsuite.xml names every case in
    upper case, but six metafiles are stored lower case (bigcgm04, edgstl01,
    polybz01..03). Matching on exact spelling silently loses those on a
    case-sensitive filesystem, so index the directory once instead.
    """
    index = {}
    for pattern in ("*.cgm", "*.CGM"):
        for path in static.glob(pattern):
            index.setdefault(path.name.lower(), path)
    return index


def svg_text(svg: Path) -> str:
    try:
        raw = svg.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""
    return " ".join(TEXTCONTENT.findall(raw))


def evaluate(case, cli: Path, suite: Path, profile: str, workdir: Path):
    static = suite / "static10"
    cgm = metafile_index(static).get((case["name"] + ".cgm").lower())

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


def _data_uri(path: Path) -> str:
    return "data:image/png;base64," + base64.b64encode(
        path.read_bytes()).decode("ascii")


WORKSHEET_CSS = """
:root{--bg:#fff;--fg:#111;--muted:#5a5a5a;--line:#d8d8d8;--card:#fff;
      --pass:#137333;--fail:#c5221f;--accent:#1a56db}
@media (prefers-color-scheme:dark){:root{--bg:#16181c;--fg:#e8e8e8;
      --muted:#a0a0a0;--line:#333;--card:#1e2126;--pass:#5bb974;--fail:#f28b82;
      --accent:#8ab4f8}}
*{box-sizing:border-box}
body{font:14px/1.55 system-ui,-apple-system,sans-serif;margin:0;
     background:var(--bg);color:var(--fg)}
header{position:sticky;top:0;z-index:10;background:var(--card);
       border-bottom:1px solid var(--line);padding:.7rem 1.25rem;
       display:flex;gap:1rem;align-items:center;flex-wrap:wrap}
header h1{font-size:1rem;margin:0 .75rem 0 0}
.progress{font-variant-numeric:tabular-nums;color:var(--muted)}
.bar{height:6px;background:var(--line);border-radius:3px;width:170px;
     overflow:hidden}
.bar>i{display:block;height:100%;background:var(--accent);width:0}
main{padding:1.25rem;max-width:1180px;margin:0 auto}
h2{margin:2.25rem 0 .75rem;border-bottom:2px solid var(--line);
   padding-bottom:.3rem;font-size:1.05rem}
.case{border:1px solid var(--line);border-radius:8px;padding:1rem;
      margin:1rem 0;background:var(--card)}
.case h3{margin:0 0 .25rem;font-size:1rem}
.purpose{color:var(--muted);font-style:italic;margin:.25rem 0 .75rem}
.pair{display:flex;gap:1rem;flex-wrap:wrap;align-items:flex-start}
.pair figure{margin:0}
.pair img{width:400px;max-width:100%;border:1px solid var(--line);
          background:#fff;display:block}
figcaption{font-size:12px;color:var(--muted);padding-top:.25rem}
.pair.stack{position:relative;width:400px;max-width:100%}
.pair.stack figure{position:absolute;inset:0;margin:0}
.pair.stack figure:first-child{position:relative}
.pair.stack figcaption{display:none}
ol{margin:.75rem 0 0;padding-left:1.4rem}
li{margin:.3rem 0}
li.auto-pass{color:var(--pass)}
li.auto-fail{color:var(--fail);font-weight:600}
.ctl{margin-left:.5rem;white-space:nowrap}
.ctl button{font:inherit;font-size:12px;border:1px solid var(--line);
   background:transparent;color:var(--fg);border-radius:4px;padding:1px 7px;
   cursor:pointer;margin-right:2px}
.ctl button[aria-pressed=true][data-v=pass]{background:var(--pass);
   border-color:var(--pass);color:#fff}
.ctl button[aria-pressed=true][data-v=fail]{background:var(--fail);
   border-color:var(--fail);color:#fff}
button.plain{font:inherit;border:1px solid var(--line);background:transparent;
   color:var(--fg);border-radius:5px;padding:3px 10px;cursor:pointer}
.hide{display:none}
textarea{width:100%;height:150px;font:12px/1.4 ui-monospace,monospace;
   background:var(--bg);color:var(--fg);border:1px solid var(--line);
   border-radius:6px;padding:.5rem;margin-bottom:1rem}
.note{color:var(--muted);font-size:13px}
"""

WORKSHEET_JS = """
const KEY='opencgm-static10-verdicts';
let V={};
try{V=JSON.parse(localStorage.getItem(KEY)||'{}')}catch(e){V={}}
const save=()=>{try{localStorage.setItem(KEY,JSON.stringify(V))}catch(e){}};
const ctls=[...document.querySelectorAll('.ctl')];
const total=ctls.length;

function applyFilter(){
  const f=document.getElementById('filter').value;
  document.querySelectorAll('.case').forEach(card=>{
    const cs=[...card.querySelectorAll('.ctl')];
    let show=true;
    if(f==='todo') show=cs.some(c=>!V[c.dataset.id]);
    else if(f==='fail') show=cs.some(c=>V[c.dataset.id]==='fail');
    card.classList.toggle('hide',!show);
  });
  document.querySelectorAll('h2').forEach(h=>{
    let n=h.nextElementSibling, any=false;
    while(n && n.tagName!=='H2'){
      if(n.classList.contains('case') && !n.classList.contains('hide')) any=true;
      n=n.nextElementSibling;
    }
    h.classList.toggle('hide',!any);
  });
}

function refresh(){
  let n=0;
  ctls.forEach(c=>{
    const v=V[c.dataset.id];
    if(v)n++;
    c.querySelectorAll('button').forEach(b=>
      b.setAttribute('aria-pressed', String(b.dataset.v===v)));
  });
  document.getElementById('count').textContent=n+' / '+total+' adjudicated';
  document.getElementById('barfill').style.width=(total?100*n/total:0)+'%';
  applyFilter();
}

document.addEventListener('click',e=>{
  const b=e.target.closest('.ctl button'); if(!b)return;
  const id=b.parentElement.dataset.id;
  if(V[id]===b.dataset.v){delete V[id];}else{V[id]=b.dataset.v;}
  save(); refresh();
});
document.getElementById('filter').addEventListener('change',applyFilter);
document.getElementById('overlay').addEventListener('change',e=>{
  document.querySelectorAll('.pair').forEach(p=>
    p.classList.toggle('stack',e.target.checked));
  document.getElementById('mixwrap').classList.toggle('hide',!e.target.checked);
});
document.getElementById('mix').addEventListener('input',e=>{
  document.querySelectorAll('.pair figure:last-child img').forEach(i=>
    i.style.opacity=e.target.value);
});
document.getElementById('export').addEventListener('click',()=>{
  const rows=ctls.map(c=>({test:c.dataset.test,checkpoint:c.dataset.cp,
    verdict:V[c.dataset.id]||'unadjudicated'}));
  const t=document.getElementById('out');
  t.classList.remove('hide');
  t.value=JSON.stringify(rows,null,2);
  t.focus(); t.select();
});
document.getElementById('reset').addEventListener('click',()=>{
  if(confirm('Discard all recorded verdicts?')){V={};save();refresh();}
});
refresh();
"""


def build_worksheet(dest: Path, suite: Path, rows, embed: bool = False):
    """Emit an operator worksheet for the checkpoints needing human judgement.

    Each case shows the suite's reference image beside this converter's
    rendering of the same metafile. Checkpoints already decided mechanically
    are marked; the rest get pass/fail controls whose verdicts persist in the
    browser, so a session survives a reload and can be exported as JSON.
    """
    dest.mkdir(parents=True, exist_ok=True)
    render_dir = dest / "render"
    render_dir.mkdir(exist_ok=True)
    refs = dest / "reference"
    refs.mkdir(exist_ok=True)

    marks = {"pass": "&#10003;", "fail": "&#10007;", "n/a": "&mdash;"}
    parts = [
        "<!doctype html><html lang='en'><head><meta charset='utf-8'>",
        "<meta name='viewport' content='width=device-width,initial-scale=1'>",
        "<title>OpenCGM - WebCGM static10 operator worksheet</title>",
        "<style>" + WORKSHEET_CSS + "</style></head><body>",
        "<header><h1>WebCGM static10 &mdash; operator worksheet</h1>",
        "<span class='progress' id='count'>0 / 0</span>",
        "<span class='bar'><i id='barfill'></i></span>",
        "<label>Show <select id='filter'>",
        "<option value='all'>all cases</option>",
        "<option value='todo'>unadjudicated</option>",
        "<option value='fail'>marked failing</option></select></label>",
        "<label><input type='checkbox' id='overlay'> overlay</label>",
        "<span id='mixwrap' class='hide'>blend "
        "<input type='range' id='mix' min='0' max='1' step='0.05' value='0.5'>"
        "</span>",
        "<button class='plain' id='export'>Export JSON</button>",
        "<button class='plain' id='reset'>Reset</button>",
        "</header><main>",
        "<p class='note'>Left is the suite's reference image; right is this "
        "converter's rendering of the same metafile. Tick <b>overlay</b> to "
        "stack them and use the blend slider to spot differences. Checkpoints "
        "already decided mechanically are marked. Verdicts you record are kept "
        "in this browser and survive a reload.</p>",
        "<textarea id='out' class='hide' readonly></textarea>",
    ]

    by_cat = collections.defaultdict(list)
    for r in rows:
        by_cat[r["case"]["category"]].append(r)

    for cat in sorted(by_cat):
        parts.append("<h2>" + html.escape(cat) + "</h2>")
        for r in by_cat[cat]:
            case = r["case"]
            name = case["name"]
            parts.append("<div class='case'><h3>" + html.escape(name) + "</h3>")
            if case["purpose"]:
                parts.append("<p class='purpose'>"
                             + html.escape(case["purpose"]) + "</p>")

            ref_src = suite / "static10" / "images" / (name + ".png")
            png = render_dir / (name + ".png")
            ref_uri = None
            out_uri = None
            if ref_src.is_file():
                if embed:
                    ref_uri = _data_uri(ref_src)
                else:
                    shutil.copyfile(ref_src, refs / ref_src.name)
                    ref_uri = "reference/" + ref_src.name
            if png.is_file():
                out_uri = _data_uri(png) if embed else "render/" + png.name

            if ref_uri or out_uri:
                parts.append("<div class='pair'>")
                if ref_uri:
                    parts.append("<figure><img src='" + ref_uri
                                 + "' alt='reference rendering'>"
                                 "<figcaption>reference (suite)</figcaption>"
                                 "</figure>")
                if out_uri:
                    parts.append("<figure><img src='" + out_uri
                                 + "' alt='OpenCGM rendering'>"
                                 "<figcaption>OpenCGM</figcaption></figure>")
                parts.append("</div>")

            parts.append("<ol>")
            for idx, res in enumerate(r["results"]):
                text = html.escape(res["text"])
                if res["verdict"] == "operator":
                    cid = name + "#" + str(idx)
                    parts.append(
                        "<li>" + text + "<span class='ctl' data-id='"
                        + html.escape(cid) + "' data-test='"
                        + html.escape(name) + "' data-cp='"
                        + html.escape(res["text"][:120]) + "'>"
                        "<button data-v='pass' aria-pressed='false'>pass</button>"
                        "<button data-v='fail' aria-pressed='false'>fail</button>"
                        "</span></li>")
                else:
                    cls = {"pass": "auto-pass", "fail": "auto-fail"}.get(
                        res["verdict"], "")
                    extra = ""
                    if res["detail"]:
                        extra = " <em>" + html.escape(res["detail"]) + "</em>"
                    parts.append("<li class='" + cls + "'>"
                                 + marks.get(res["verdict"], "") + " " + text
                                 + extra + "</li>")
            parts.append("</ol></div>")

    parts.append("</main><script>" + WORKSHEET_JS + "</script></body></html>")
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
    ap.add_argument("--embed", action="store_true",
                    help="inline images as data URIs so the "
                         "worksheet is one self-contained file")
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
        "| not evaluated (metafile absent) | {} |".format(tally["n/a"]),
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
    if tally["n/a"]:
        missing = sorted({r["case"]["name"] for r in rows
                          for res in r["results"] if res["verdict"] == "n/a"})
        print("WARNING: {} checkpoints not evaluated, metafile missing for: {}"
              .format(tally["n/a"], ", ".join(missing)))
    print("wrote {}".format(args.out))

    if args.worksheet:
        build_worksheet(args.worksheet, args.suite, rows, args.embed)
        print("wrote {}".format(args.worksheet / "index.html"))

    # Non-zero exit on any mechanically decided failure, so this can gate CI.
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
