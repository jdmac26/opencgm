# Security Policy

## Reporting a vulnerability

Please report security issues privately rather than opening a public issue.

Use GitHub's private vulnerability reporting:
[**Report a vulnerability**](../../security/advisories/new).

Please include a description of the issue, the affected version or commit, and
a reproducer where possible. You can expect an acknowledgement within a few
business days.

## Scope

OpenCGM parses untrusted binary input. CGM is a complex binary format, and the
parser is the primary security surface. Reports of the following are in scope
and taken seriously:

- Memory-safety issues reachable from a malformed CGM file — out-of-bounds
  reads or writes, use-after-free, type confusion
- Unbounded allocation or non-terminating loops triggered by crafted input
  (`include/opencgm/security_limits.h` defines the guards intended to prevent
  this; a bypass is a valid report)
- Path traversal or unintended file writes via the CLI's output handling
- Issues in the SVG emitter that could produce output capable of executing
  script in a downstream viewer

Out of scope: crashes in debug and diagnostic tools built with
`BUILD_DEBUG_TOOLS`, and issues that require an attacker to already control the
local machine.

## Handling untrusted files

If you convert CGM files from untrusted sources, run the converter in a sandbox
with restricted filesystem access. The parser enforces the limits in
`security_limits.h`, but defence in depth is appropriate for any binary parser.
