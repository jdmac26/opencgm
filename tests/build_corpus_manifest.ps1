# Workstream A bootstrap: seed docs/corpus-manifest.json from the Default-profile
# output sweep (AllOn baseline). Hand-audit required after generation to fill in
# cgm_profile, xcf split, and region_shapes_present.
#
# Usage: pwsh engine/tests/build_corpus_manifest.ps1 [-OutFile <path>]

param(
    [string] $DefaultDir = "$PSScriptRoot/../samples/input/profile-test/Default",
    [string] $SourceDir  = "$PSScriptRoot/../samples/input",
    [string] $OutFile    = "$PSScriptRoot/../../docs/corpus-manifest.json"
)

$ErrorActionPreference = 'Stop'

# Map observed-in-output attribute name -> canonical preservable-attribute key.
# Both S1000D compact (data-apsid) and WebCGM dashed (data-aps-id) forms collapse
# to the same canonical key so the manifest is profile-naming-agnostic.
# Ordered longest-first so the word-boundary scan matches the more specific
# form before the generic one (e.g. data-aps-region-owner before data-aps-region).
$attrMap = [ordered] @{
    'data-aps-region-owner' = '__internal__'
    'data-aps-region-shape' = '__internal__'
    'data-aps-region-user-visible' = '__internal__'
    'data-aps-effective-visible'   = '__internal__'
    'data-aps-user-visible'        = '__internal__'
    'data-aps-visible'             = '__internal__'
    'data-aps-embed'               = '__internal__'
    'data-aps-viewcontext'  = 'viewcontext'
    'data-viewcontext'      = 'viewcontext'
    'data-aps-linktitle'    = 'linktitle'
    'data-linktitle'        = 'linktitle'
    'data-aps-screentip'    = 'screentip'
    'data-screentip'        = 'screentip'
    'data-aps-linkuri'      = 'linkuri'
    'data-linkuri'          = 'linkuri'
    'data-aps-region'       = 'region'
    'data-aps-layer'        = 'layer'
    'data-apsname'          = 'apsname'
    'data-aps-name'         = 'apsname'
    'data-aps-type'         = 'aps_type'
    'data-apstype'          = 'aps_type'
    'data-apsid'            = 'apsid'
    'data-aps-id'           = 'apsid'
    'data-layer'            = 'layer'
}

# Extract distinct canonical attribute keys present in a single SVG file.
function Get-CanonicalAttrs {
    param([string] $Path)

    $attrs = [System.Collections.Generic.HashSet[string]]::new()
    $text = Get-Content -Raw -LiteralPath $Path

    foreach ($name in $attrMap.Keys) {
        # Word-boundary match on attribute name followed by '='.
        # Internal-marker values are ignored — they represent engine-emitted
        # plumbing attributes, not preservable APS attributes.
        if ($text -match "(?<![\w-])$name=") {
            $canonical = $attrMap[$name]
            if ($canonical -ne '__internal__') {
                [void] $attrs.Add($canonical)
            }
        }
    }

    return @($attrs | Sort-Object)
}

# Resolve an XCF companion path if one exists next to the CGM.
function Resolve-Xcf {
    param([string] $CgmPath)

    $base = [System.IO.Path]::GetFileNameWithoutExtension($CgmPath)
    $dir  = [System.IO.Path]::GetDirectoryName($CgmPath)
    foreach ($ext in '.xcf', '.companion.xml', '.xml') {
        $candidate = Join-Path $dir ($base + $ext)
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    return $null
}

# PS 5.1's ConvertTo-Json has well-known bugs with single-element arrays
# (collapses to scalar) and empty arrays (renders as {}). Build JSON manually
# to keep the manifest schema stable and human-reviewable.
function ConvertTo-JsonStringArray {
    param([string[]] $items)
    if (-not $items -or $items.Count -eq 0) { return '[]' }
    $escaped = $items | ForEach-Object { '"' + ($_ -replace '\\', '\\\\' -replace '"', '\"') + '"' }
    return '[' + ($escaped -join ', ') + ']'
}

function ConvertTo-JsonString {
    param($s)
    if ($null -eq $s -or $s -eq '') {
        return 'null'
    }
    return '"' + ($s -replace '\\', '\\\\' -replace '"', '\"') + '"'
}

function ConvertTo-JsonStringOrEmpty {
    param([string] $s)
    # For the sample filename itself — always quoted, never null.
    return '"' + ($s -replace '\\', '\\\\' -replace '"', '\"') + '"'
}

$entries = Get-ChildItem -LiteralPath $DefaultDir -Filter '*.svg' | Sort-Object Name

$sb = [System.Text.StringBuilder]::new()
[void] $sb.AppendLine('{')

$first = $true
foreach ($svg in $entries) {
    $base = [System.IO.Path]::GetFileNameWithoutExtension($svg.Name)
    $cgmName = $base + '.CGM'
    $cgmPath = Join-Path $SourceDir $cgmName
    if (-not (Test-Path -LiteralPath $cgmPath)) {
        Write-Warning "CGM not found for $($svg.Name): $cgmPath"
        continue
    }

    $cgmAttrs = Get-CanonicalAttrs -Path $svg.FullName
    $xcfPath  = Resolve-Xcf -CgmPath $cgmPath
    $xcfPresent = [bool] $xcfPath
    $xcfName = if ($xcfPresent) { [System.IO.Path]::GetFileName($xcfPath) } else { $null }

    # ICN-* filename convention is S1000D-specific (ICN = Illustration Control
    # Number per S1000D Chapter 4.3). Default every real-corpus sample to
    # "s1000d" — override by hand in the JSON if a sample is sourced from a
    # different profile family.
    $cgmProfile = if ($base -like 'ICN-*') { 's1000d' } else { 'unknown' }

    if (-not $first) { [void] $sb.AppendLine(',') }
    $first = $false

    [void] $sb.Append("  ").Append((ConvertTo-JsonStringOrEmpty $cgmName)).AppendLine(': {')
    [void] $sb.Append('    "cgm_profile": ').Append((ConvertTo-JsonStringOrEmpty $cgmProfile)).AppendLine(',')
    [void] $sb.Append('    "cgm_attributes": ').Append((ConvertTo-JsonStringArray $cgmAttrs)).AppendLine(',')
    [void] $sb.Append('    "xcf_present": ').Append($(if ($xcfPresent) { 'true' } else { 'false' })).AppendLine(',')
    [void] $sb.Append('    "xcf_path": ').Append((ConvertTo-JsonString $xcfName)).AppendLine(',')
    [void] $sb.AppendLine('    "xcf_attributes": [],')
    [void] $sb.Append('    "expected_output_attributes": ').Append((ConvertTo-JsonStringArray $cgmAttrs)).AppendLine(',')
    # region_shapes_present cannot be determined from current SVG output (every
    # region renders as <polygon> pre-Workstream-E). Real-corpus shape coverage
    # is deferred to CGM-parser-level inspection; synthetic fixtures (see
    # engine/samples/input/synthetic/) provide per-shape ground truth.
    [void] $sb.AppendLine('    "region_shapes_present": ["unknown"],')
    [void] $sb.AppendLine('    "notes": "seeded from Default-profile output sweep"')
    [void] $sb.Append('  }')
}

[void] $sb.AppendLine().AppendLine('}')
Set-Content -LiteralPath $OutFile -Value $sb.ToString() -Encoding utf8

# Coverage audit — count covering samples per preservable attribute.
$preservable = @('apsid', 'apsname', 'aps_type', 'linkuri', 'linktitle',
                 'region', 'screentip', 'viewcontext', 'layer')
$counts = @{}
foreach ($p in $preservable) { $counts[$p] = 0 }

$parsed = Get-Content -Raw -LiteralPath $OutFile | ConvertFrom-Json
$parsedProps = $parsed.PSObject.Properties
foreach ($prop in $parsedProps) {
    foreach ($attr in $prop.Value.expected_output_attributes) {
        if ($counts.ContainsKey($attr)) { $counts[$attr]++ }
    }
}

Write-Host ""
Write-Host "Coverage audit (>= 3 samples per preservable attribute):"
$gaps = @()
foreach ($p in $preservable) {
    $n = $counts[$p]
    $status = if ($n -ge 3) { '[OK]  ' } else { '[GAP] ' }
    Write-Host ("  {0} {1,-14} {2} samples" -f $status, $p, $n)
    if ($n -lt 3) { $gaps += $p }
}
if ($gaps.Count -gt 0) {
    Write-Host ""
    Write-Host "Gaps requiring synthetic fixtures: $($gaps -join ', ')"
}

Write-Host "Seeded manifest: $OutFile"
Write-Host "Entries: $($entries.Count)"
Write-Host ""
Write-Host "Hand-audit required:"
Write-Host "  * cgm_profile per sample (s1000d / webcgm / cals / pip / ata / unknown)"
Write-Host "  * region_shapes_present per sample (from CGM source inspection)"
Write-Host "  * xcf_attributes / expected_output_attributes where XCF is present"
