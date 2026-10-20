#!/usr/bin/env python3
"""Check that an unpacked release archive works on its own.

Run against the extracted archive, not the build tree, so that nothing the
build left behind (an RPATH into build/lib, a DLL beside the build output, an
installed Visual C++ runtime) can mask a packaging defect. It checks that:

  * bin/opencgm_cli reports the expected version;
  * it converts a small generated metafile to well-formed SVG;
  * cgm_validate runs;
  * the binaries depend on nothing a fresh machine lacks: on Windows, no
    Visual C++ / UCRT runtime imports (the archive is built with the runtime
    linked statically); on Linux, every shared library resolves, and
    libopencgm.so resolves from inside the package.

Usage:
    python scripts/smoke-test-package.py <unpacked-dir> [--expect-version 1.5.1]
"""

from __future__ import annotations

import argparse
import os
import re
import struct
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

# DLLs a clean Windows install does not guarantee. Windows 10+ ships the UCRT,
# but the archive links it statically too, so any of these is a regression.
WINDOWS_RUNTIME_DLLS = re.compile(
    r"^(vcruntime\d+.*|msvcp\d+.*|concrt\d+|ucrtbase|api-ms-win-crt-.*)\.dll$",
    re.IGNORECASE)


def minimal_cgm() -> bytes:
    """A binary metafile with a circle and a line of restricted text."""
    out = bytearray()

    def i16(v: int) -> bytes:
        return struct.pack(">h", v)

    def element(cls: int, eid: int, params: bytes = b"") -> None:
        n = len(params)
        out.extend(struct.pack(">H", (cls << 12) | (eid << 5) | min(n, 31)))
        if n >= 31:
            out.extend(struct.pack(">H", n))
        out.extend(params)
        if n % 2:
            out.append(0)

    def string(s: str) -> bytes:
        return bytes([len(s)]) + s.encode("ascii")

    element(0, 1, string("smoke"))                      # BEGIN METAFILE
    element(1, 1, i16(1))                               # METAFILE VERSION
    element(0, 3, string("p1"))                         # BEGIN PICTURE
    element(2, 6, i16(0) + i16(0) + i16(1000) + i16(1000))  # VDC EXTENT
    element(0, 4)                                       # BEGIN PICTURE BODY
    element(4, 12, i16(500) + i16(500) + i16(200))      # CIRCLE
    element(5, 15, i16(20))                             # CHARACTER HEIGHT
    element(4, 5, i16(300) + i16(30) + i16(100) + i16(100) + i16(1)
            + string("OpenCGM"))                        # RESTRICTED TEXT
    element(0, 5)                                       # END PICTURE
    element(0, 2)                                       # END METAFILE
    return bytes(out)


def pe_imports(path: Path) -> list[str]:
    """Names of the DLLs a PE image imports directly."""
    data = path.read_bytes()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError(f"{path.name}: not a PE image")
    coff = pe + 4
    n_sections = struct.unpack_from("<H", data, coff + 2)[0]
    opt_size = struct.unpack_from("<H", data, coff + 16)[0]
    opt = coff + 20
    magic = struct.unpack_from("<H", data, opt)[0]
    dirs = opt + (112 if magic == 0x20B else 96)
    import_rva = struct.unpack_from("<I", data, dirs + 8)[0]
    if import_rva == 0:
        return []

    sections = []
    for i in range(n_sections):
        s = opt + opt_size + 40 * i
        vsize, vaddr, rsize, raddr = struct.unpack_from("<IIII", data, s + 8)
        sections.append((vaddr, max(vsize, rsize), raddr))

    def offset(rva: int) -> int:
        for vaddr, size, raddr in sections:
            if vaddr <= rva < vaddr + size:
                return rva - vaddr + raddr
        raise ValueError(f"{path.name}: RVA {rva:#x} outside every section")

    names = []
    desc = offset(import_rva)
    while True:
        fields = struct.unpack_from("<IIIII", data, desc)
        if not any(fields):
            break
        name_off = offset(fields[3])
        names.append(data[name_off:data.index(b"\0", name_off)].decode("ascii"))
        desc += 20
    return names


def check_windows(binaries: list[Path]) -> list[str]:
    problems = []
    for b in binaries:
        imports = pe_imports(b)
        bad = [n for n in imports if WINDOWS_RUNTIME_DLLS.match(n)]
        print(f"  {b.name}: imports {', '.join(sorted(imports, key=str.lower))}")
        if bad:
            problems.append(f"{b.name} needs the Visual C++ runtime: {', '.join(bad)}")
    return problems


def check_linux(binaries: list[Path], package: Path) -> list[str]:
    problems = []
    env = {k: v for k, v in os.environ.items() if k != "LD_LIBRARY_PATH"}
    for b in binaries:
        ldd = subprocess.run(["ldd", str(b)], capture_output=True, text=True, env=env)
        for line in ldd.stdout.splitlines():
            if "not found" in line:
                problems.append(f"{b.name}: {line.strip()}")
            m = re.match(r"\s*libopencgm\.so\S*\s+=>\s+(\S+)", line)
            if m and package.resolve() not in Path(m.group(1)).resolve().parents:
                problems.append(f"{b.name} loads libopencgm from outside the package: {m.group(1)}")
        objdump = subprocess.run(["objdump", "-T", str(b)], capture_output=True, text=True)
        glibc = sorted({tuple(map(int, v.split("."))) for v in re.findall(r"GLIBC_(\d+(?:\.\d+)+)", objdump.stdout)})
        if glibc:
            print(f"  {b.name}: requires glibc >= {'.'.join(map(str, glibc[-1]))}")
    return problems


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("package", type=Path, help="unpacked archive root (contains bin/)")
    ap.add_argument("--expect-version", help="version opencgm_cli must report")
    args = ap.parse_args()

    exe = ".exe" if os.name == "nt" else ""
    bindir = args.package / "bin"
    cli = bindir / f"opencgm_cli{exe}"
    validate = bindir / f"cgm_validate{exe}"
    problems = []
    for tool in (cli, validate):
        if not tool.is_file():
            problems.append(f"missing {tool.relative_to(args.package)}")
    if problems:
        print("\n".join(problems), file=sys.stderr)
        return 1

    print("dependencies")
    if os.name == "nt":
        binaries = sorted([*bindir.glob("*.exe"), *bindir.glob("*.dll")])
        problems += check_windows(binaries)
    else:
        binaries = [cli, validate, *sorted((args.package / "lib").glob("libopencgm.so*"))]
        problems += check_linux(binaries, args.package)

    version = subprocess.run([str(cli), "--version"], capture_output=True, text=True)
    print(f"version: {version.stdout.strip() or version.stderr.strip()}")
    if version.returncode != 0:
        problems.append(f"opencgm_cli --version exited {version.returncode}")
    elif args.expect_version and version.stdout.split()[-1] != args.expect_version:
        problems.append(f"opencgm_cli reports {version.stdout.strip()!r}, expected {args.expect_version}")

    with tempfile.TemporaryDirectory() as td:
        cgm, svg = Path(td) / "smoke.cgm", Path(td) / "smoke.svg"
        cgm.write_bytes(minimal_cgm())
        conv = subprocess.run([str(cli), str(cgm), str(svg)], capture_output=True, text=True)
        if conv.returncode != 0 or not svg.is_file():
            problems.append(f"conversion failed ({conv.returncode}): {conv.stderr.strip()[:300]}")
        else:
            root = ET.parse(svg).getroot()
            tags = {el.tag.rsplit("}", 1)[-1] for el in root.iter()}
            print(f"converted: {svg.stat().st_size} bytes, elements {sorted(tags)}")
            if "text" not in tags or not ({"ellipse", "circle", "path"} & tags):
                problems.append("converted SVG is missing the circle or the text")

        val = subprocess.run([str(validate), str(cgm)], capture_output=True, text=True)
        if "Summary" not in val.stdout:
            problems.append(f"cgm_validate did not produce a report (exit {val.returncode})")
        dump = subprocess.run([str(validate), "--dump", str(cgm)], capture_output=True, text=True)
        if dump.returncode != 0 or "4/12\tCircle" not in dump.stdout:
            problems.append(f"cgm_validate --dump did not list the circle (exit {dump.returncode})")
        assigned = subprocess.run([str(cli), "--profile=compat", str(cgm), str(svg)], capture_output=True, text=True)
        if assigned.returncode != 0:
            problems.append(f"opencgm_cli rejected --profile=compat (exit {assigned.returncode})")

    if problems:
        print("\nFAILED", file=sys.stderr)
        for p in problems:
            print(f"  {p}", file=sys.stderr)
        return 1
    print("\npackage OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
