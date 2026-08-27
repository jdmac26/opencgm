# opencgm_cli — native CGM-to-SVG CLI

`opencgm_cli` is the C++ command-line front end for the OpenCGM engine. It ships alongside the GUI installer on Windows and as a standalone binary on Linux.

This is the native engine CLI. The higher-level managed CLI (`opencgm-cli.exe`) wraps it with licence enforcement and .NET-side post-processing — the managed CLI is the one most users should call.

---

## Usage

```
opencgm_cli [options] <input.cgm> <output.svg>
opencgm_cli --watch <dir> --output-dir <dir> [options]
```

Run `opencgm_cli --help` for the full option list.

---

## Presets

`--preset <name>` applies one of the seven built-in output presets. Each preset sets 18 core knobs — CGM profile, DPI, hotspot encoding, region handling, multi-link mode, APS preservation flags (id / name / link URI / link title / region / layer / screentip / view-context / APS type), validation, text-as-path, minify, strict compliance — to match the GUI preset of the same name.

| Preset name | Aliases | GUI preset | Typical target |
|-------------|---------|------------|----------------|
| `s1000d` | — | S1000D | Defence / civil aviation IETP |
| `ataispec2200` | `ata`, `ataispec` | ATA iSpec 2200 | Commercial aviation legacy |
| `webcgm21` | `webcgm` | WebCGM 2.1 | Web technical graphics |
| `cals` | — | CALS | MIL-PRF-28003 U.S. DoD |
| `pipcggc` | `pip` | PIP / CGGC | Petroleum seismic / well-log |
| `defaultbalanced` | `default` | Default (balanced) | Unknown target |
| `highqualityprint` | `highquality`, `print` | High Quality Print | 600 DPI print |

Individual flags (`--text-as-path`, `--no-tcc`, `--font-map`, etc.) applied alongside `--preset` override the preset's value for that knob.

---

## Examples

Single-file conversion:

```
opencgm_cli --preset s1000d figure.cgm figure.svg
```

Watch folder:

```
opencgm_cli --watch ./incoming --output-dir ./out --preset webcgm21
```

Print-fidelity:

```
opencgm_cli --preset highqualityprint --validate-geometry figure.cgm figure.svg
```

---

## Licence

On Windows, `opencgm_cli` reads the encrypted licence file at `%LOCALAPPDATA%\OpenCGM\license.dat`, written by the GUI. No separate CLI activation is required — activate once in the GUI and `opencgm_cli` picks up the same licence. (Before v0.6.1 the CLI read from `%APPDATA%\CgmToSvgConverter\license.dat`; users on the old path need to re-activate.)

On Linux, the CLI reads the licence key from the `OPENCGM_LICENSE_KEY` environment variable.

---

## Related docs

- [`opencgm_cli --help`](opencgm_cli.cpp) — the full `--preset` + `--profile` matrix.
