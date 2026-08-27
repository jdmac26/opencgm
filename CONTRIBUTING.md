# Contributing

Contributions are welcome. CGM is a large specification with a very long tail of
real-world encoder quirks, so the most valuable contributions are usually the
narrow ones: a file that renders wrong, and a test that pins the correct
behaviour.

## Reporting a conversion bug

A good report includes:

1. The CGM file, or a minimal reduction of it. If the file cannot be shared,
   `cgm_validate --dump` output plus a description of the affected element is
   usually enough to work from.
2. The command line used, including the `--profile`.
3. What the output looks like versus what it should look like.
4. The producing application, if known — `IsoDraw`, `Arbortext`, `CorelDRAW`,
   and `Illustrator` all emit CGM with different characteristic quirks, and
   naming it often identifies the bug immediately.

Please do not attach files you are not permitted to redistribute. Aerospace and
defence CGMs are frequently covered by NDA or export control; a synthetic
reduction is always preferable.

## Development

```bash
cmake -B build
cmake --build build
ctest --test-dir build
```

Corpus-driven tests need sample data, which is not vendored:

```bash
python scripts/fetch-testdata.py
export OPENCGM_SAMPLES_DIR=$PWD/testdata
```

Tests requiring a corpus skip cleanly when it is absent, so a bare `ctest` run
should be green before you start.

## Pull requests

- **Add a test.** Conversion changes need a regression test — either a unit test
  or a case in the parameterised suite. A fix without a test tends to regress.
- **Keep the style of the surrounding code.** The codebase is consistent; match
  it rather than reformatting.
- **One logical change per PR.** Spec-conformance fixes and refactors are much
  easier to review separately.
- **Note the spec clause.** When a change implements or corrects behaviour
  defined in ISO/IEC 8632 or WebCGM 2.1, cite the clause in the PR description.

## Profile changes

Output profiles (`s1000d`, `webcgm`, `compat`) encode compatibility decisions
that downstream publication pipelines depend on. Changes to profile defaults
need a rationale grounded in the relevant specification, and will usually need a
conformance-matrix run to show what moved.

## Licensing

By contributing, you agree that your contributions are licensed under the
Apache License 2.0, consistent with the rest of the project.
