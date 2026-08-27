#!/usr/bin/env python3
"""Fetch the sample corpora used by the corpus-driven tests.

Neither corpus is vendored in this repository. Both are third-party data
distributed under their own terms:

  * The WebCGM 2.1 Conformance Test Suite is redistributable but explicitly
    grants no right to create derivatives, so it is downloaded verbatim from
    OASIS rather than copied into the tree.

  * The S1000D Bike Data Set is published through the S1000D users' portal,
    which requires registration. It cannot be fetched unattended; this script
    prints instructions for it.

Tests that need a corpus call GTEST_SKIP() when it is missing, so the suite is
green without running this.

Usage:
    python scripts/fetch-testdata.py [--dest DIR]
"""

from __future__ import annotations

import argparse
import hashlib
import shutil
import sys
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

WEBCGM_URL = (
    "https://docs.oasis-open.org/webcgm/test-materials/webcgm21ts/"
    "webcgm21-ts-20100419.zip"
)
WEBCGM_SHA256 = "d540a452d989091db3abd83724ab9d0d9730f57ad792f4db85a04d93103063c9"
WEBCGM_DIRNAME = "webcgm21-ts"

S1000D_DIRNAME = "input"
S1000D_INSTRUCTIONS = """\
The S1000D Bike Data Set could not be fetched automatically.

It is distributed through the S1000D users' portal, which requires a (free)
account, so it cannot be downloaded unattended:

    https://users.s1000d.org/

Download the Bike Data Set, then place the CGM files here:

    {dest}

Use of this data is subject to the S1000D Terms and Conditions
(https://s1000d.org). Copyright is retained by ASD/AIA/AIAC.
"""


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def download(url: str, dest: Path) -> None:
    print(f"  downloading {url}")
    try:
        with urllib.request.urlopen(url, timeout=120) as response:
            total = response.headers.get("Content-Length")
            total_mb = f"{int(total) / 1_048_576:.1f} MB" if total else "unknown size"
            print(f"  {total_mb}")
            with dest.open("wb") as handle:
                shutil.copyfileobj(response, handle)
    except urllib.error.URLError as exc:
        raise SystemExit(f"error: download failed: {exc}") from exc


def fetch_webcgm(dest_root: Path) -> bool:
    target = dest_root / WEBCGM_DIRNAME
    if (target / "1stReadMe.html").exists():
        print(f"WebCGM 2.1 TS: already present at {target}")
        return True

    print("WebCGM 2.1 Conformance Test Suite")
    archive = dest_root / "webcgm21-ts.zip"
    dest_root.mkdir(parents=True, exist_ok=True)
    download(WEBCGM_URL, archive)

    actual = sha256_of(archive)
    if actual != WEBCGM_SHA256:
        archive.unlink(missing_ok=True)
        raise SystemExit(
            "error: checksum mismatch for the WebCGM test suite\n"
            f"  expected {WEBCGM_SHA256}\n"
            f"  actual   {actual}\n"
            "Refusing to extract. The upstream archive may have been revised; "
            "verify against https://docs.oasis-open.org/webcgm/test-materials/"
            "webcgm21ts/ before updating WEBCGM_SHA256."
        )
    print("  checksum OK")

    target.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive) as zf:
        # Guard against path traversal in the archive.
        for name in zf.namelist():
            resolved = (target / name).resolve()
            if not resolved.is_relative_to(target.resolve()):
                raise SystemExit(f"error: unsafe path in archive: {name}")
        zf.extractall(target)
    archive.unlink()
    print(f"  extracted to {target}")
    print(
        "  Copyright (c) 2002-2009, Lofton Henderson. All Rights Reserved.\n"
        "  Redistributed verbatim; no derivatives are permitted. See "
        "THIRD-PARTY-NOTICES.md."
    )
    return True


def check_s1000d(dest_root: Path) -> bool:
    target = dest_root / S1000D_DIRNAME
    cgms = list(target.glob("*.CGM")) + list(target.glob("*.cgm")) if target.exists() else []
    if cgms:
        print(f"S1000D Bike Data Set: {len(cgms)} files present at {target}")
        return True
    print("S1000D Bike Data Set: not present")
    print()
    print(S1000D_INSTRUCTIONS.format(dest=target))
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--dest",
        type=Path,
        default=Path(__file__).resolve().parent.parent / "testdata",
        help="directory to place corpora in (default: ./testdata)",
    )
    args = parser.parse_args()
    dest_root: Path = args.dest.resolve()

    print(f"Fetching test corpora into {dest_root}\n")
    fetch_webcgm(dest_root)
    print()
    complete = check_s1000d(dest_root)

    print()
    print("Point the tests at this directory:")
    print(f"    export OPENCGM_SAMPLES_DIR={dest_root}")
    print("    # PowerShell: $env:OPENCGM_SAMPLES_DIR = " f'"{dest_root}"')
    if not complete:
        print()
        print("Tests needing the S1000D corpus will skip until it is installed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
