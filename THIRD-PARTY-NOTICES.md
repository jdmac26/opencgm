# Third-Party Notices

OpenCGM incorporates material from the projects listed below. Each component
remains under its own license; the notices required by those licenses are
reproduced here in full.

---

## codessentials.CGM

OpenCGM is a C++ port of the **codessentials.CGM** C# library by Toni Wenzel.

- Project: https://github.com/twenzel/CGM
- License: MIT

```
MIT License

Copyright (c) 2019 Toni Wenzel

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## Bundled libraries

### miniz 3.1.0

Vendored at `third_party/miniz/`.

- Project: https://github.com/richgel999/miniz
- Author: Rich Geldreich
- License: public domain / Unlicense, per the header of the vendored source

### nlohmann/json

Vendored at `third_party/nlohmann/json.hpp`.

- Project: https://github.com/nlohmann/json
- SPDX-FileCopyrightText: 2013-2023 Niels Lohmann <https://nlohmann.me>
- SPDX-License-Identifier: MIT

### stb_truetype v1.26

Vendored at `third_party/stb/stb_truetype.h`.

- Project: https://github.com/nothings/stb
- Authored 2009-2021 by Sean Barrett / RAD Game Tools
- License: public domain (dual-licensed MIT)

### Skia

An optional build dependency, consumed via vcpkg (see `vcpkg.json`) and enabled
with `-DENABLE_SKIA_RENDERER=ON`. Not bundled in this repository.

- Project: https://skia.org
- Copyright (c) Google LLC
- License: BSD-3-Clause

---

## Test data

**No sample data is distributed with OpenCGM.** The corpora used by the
corpus-driven tests are third-party data under their own terms, downloaded on
demand by `scripts/fetch-testdata.py`. The notices below apply to that data once
you have fetched it.

### WebCGM 2.1 Conformance Test Suite

Downloaded verbatim from OASIS. No part of it is modified or redistributed by
this project.

- Suite: https://docs.oasis-open.org/webcgm/test-materials/webcgm21ts/
- Copyright © 2002-2009, Lofton Henderson. All Rights Reserved.

The WebCGM TS license permits use, copying, and distribution without fee or
royalty, provided that a link to the original suite and the copyright notice
above accompany all copies. **The license grants no right to create
modifications or derivative works of the WebCGM TS**, which is why the suite is
fetched rather than vendored, and why SVG produced from it by OpenCGM is kept
outside the fetched tree.

### S1000D Bike Data Set

The public example dataset published for the S1000D specification. It is
distributed through the S1000D users' portal and must be installed manually;
`scripts/fetch-testdata.py` prints instructions.

- Specification and terms of use: https://s1000d.org
- Copyright is retained by ASD/AIA/AIAC.

Use of this data is subject to the S1000D Terms and Conditions.
