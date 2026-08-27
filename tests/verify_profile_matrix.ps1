# Per-profile compliance matrix harness.
#
# Reads:
#   docs/corpus-manifest.json                    -- per-sample ground truth
#   engine/tests/rules/expected_cells.json       -- per-profile expected cells
#   engine/samples/input/profile-test/<profile>/ -- converter output
#
# Writes:
#   engine/samples/input/profile-test/_compliance-matrix.md
#   engine/samples/input/profile-test/_compliance-summary.json
#
# Exits non-zero if any sample x profile x attribute cell is __.
#
# Usage:
#   pwsh engine/tests/verify_profile_matrix.ps1 [-SkipValidation]

param(
    [string] $ManifestPath  = "$PSScriptRoot/../../docs/corpus-manifest.json",
    [string] $RulesPath     = "$PSScriptRoot/rules/expected_cells.json",
    [string] $ProfileTestDir = "$PSScriptRoot/../samples/input/profile-test",
    [switch] $SkipValidation
)

$ErrorActionPreference = 'Stop'

# Cell symbols -- ASCII-safe for Windows PowerShell default encoding.
$CELL_NA = 'n/a'
$CELL_DASH = '-'
$CELL_OK = 'OK'
$CELL_FAIL = 'FAIL'

# Load structural + profile-specific rule sets for validation lanes.
. (Join-Path $PSScriptRoot 'rules/structural_assertions.ps1')
. (Join-Path $PSScriptRoot 'rules/s1000d6_assertions.ps1')
. (Join-Path $PSScriptRoot 'rules/ata_assertions.ps1')

# Map profile name -> profile-specific assertion function (or $null for
# profiles that do not have a published SVG output profile to validate).
# Taxonomy revised 2026-07-21: Default/HighQualityPrint presets are retired;
# S1000DLegacy covers the pre-Issue-6 bare-attribute output style.
$ProfileAssertions = @{
    'S1000D'       = 'Invoke-S1000D6Assertions'
    'S1000DLegacy' = $null
    'AtaISpec2200' = 'Invoke-AtaAssertions'
    'WebCGM21'     = $null
    'CALS'         = $null
    'PIP'          = $null
}

$PRESERVABLE = @('apsid', 'apsname', 'aps_type', 'linkuri', 'linktitle',
                 'region', 'screentip', 'viewcontext', 'layer')

# Load inputs
if (-not (Test-Path -LiteralPath $ManifestPath)) {
    throw "Manifest not found: $ManifestPath (run build_corpus_manifest.ps1 first)"
}
if (-not (Test-Path -LiteralPath $RulesPath)) {
    throw "Rules not found: $RulesPath"
}
$manifest = Get-Content -Raw -LiteralPath $ManifestPath -Encoding UTF8 | ConvertFrom-Json
$rules    = Get-Content -Raw -LiteralPath $RulesPath    -Encoding UTF8 | ConvertFrom-Json

# Enumerate profiles from the rules file.
$profileNames = @($rules.profiles.PSObject.Properties | ForEach-Object { $_.Name })

# Enumerate samples from the manifest.
$sampleNames = @($manifest.PSObject.Properties | ForEach-Object { $_.Name })

function Test-AttributeInSvg {
    param(
        [string]   $SvgText,
        [string[]] $AttrNames
    )
    foreach ($attr in $AttrNames) {
        # Word-boundary match: attr name not preceded by [\w-] and followed by '='.
        $pattern = "(?<![\w-])$([regex]::Escape($attr))="
        if ($SvgText -match $pattern) {
            # For bare-attribute forms (id/class/data-name), require the match
            # be on an element whose principal class is "cgm-grobject". This is
            # how the WebCGM / PIP spec rules express the hotspot encoding:
            # the bare SVG id/class on the grobject element IS the APS identity.
            # If the element carries data-apsid / data-aps-id instead, the
            # engine is emitting the S1000D compact or WebCGM dashed form
            # rather than the spec-required bare form -- that is an engine
            # deviation we want to surface as FAIL, not silently pass.
            if ($attr -in @('id', 'class', 'data-name')) {
                # Grobject elements are identified by any of the type markers
                # the engine has emitted over time: the legacy CSS class
                # (removed from current output), the Tier 0 webcgm:type
                # attribute, or the data-aps-type attribute.
                $grobjectMatches = [regex]::Matches(
                    $SvgText,
                    '<[^>]*(class="[^"]*cgm-grobject[^"]*"|webcgm:type="grobject"|data-aps-type="grobject")[^>]*>')
                foreach ($m in $grobjectMatches) {
                    if ($m.Value -match "(?<![\w-])$([regex]::Escape($attr))=") {
                        return $true
                    }
                }
                # No permissive fallback: if the grobject element does not
                # carry the bare attr, the profile's spec-required form is
                # not emitted.
                continue
            }
            return $true
        }
    }
    return $false
}

function Get-Cell {
    param(
        $Rule,        # rule object for this (profile, attribute)
        [bool] $SampleExpects,
        [string] $SvgText,
        $ApsElements = $null  # optional per-sample APS ground truth from manifest
    )
    if (-not $Rule.enabled) { return $CELL_NA }
    if (-not $SampleExpects) { return $CELL_DASH }
    $svgAttrs = @($Rule.svg_attrs)

    # Bare-form rule inapplicability. WebCGM 2.1 Sections 3.2.1.1-.2 define the
    # bare SVG id attribute as the hotspot-identification mechanism, scoped to
    # grobject (and para/subpara for text) APS types. Layer APS carries
    # layername per Section 3.2.1.5 and is not a hotspot target. When the
    # profile's rule expects a bare-form attr (id / class / data-name) and the
    # manifest declares this sample has no grobject/linkgrobj APS, the rule
    # doesn't apply -- return dash, not FAIL. When aps_elements is missing from
    # the manifest we fall back to the strict check so new samples surface
    # correctly rather than silently passing.
    $bareForms = @('id', 'class', 'data-name')
    $hasBareForm = $false
    foreach ($a in $svgAttrs) { if ($a -in $bareForms) { $hasBareForm = $true; break } }
    if ($hasBareForm -and $null -ne $ApsElements) {
        $hasGrobject = $false
        foreach ($e in $ApsElements) {
            if ($e.type -in @('grobject', 'linkgrobj')) { $hasGrobject = $true; break }
        }
        if (-not $hasGrobject) { return $CELL_DASH }
    }

    if (Test-AttributeInSvg -SvgText $SvgText -AttrNames $svgAttrs) {
        return $CELL_OK
    }
    return $CELL_FAIL
}

# Build the matrix: map of profile -> sample -> { attr -> cell, validation }
$matrix = @{}
$fails = [System.Collections.Generic.List[string]]::new()
$validationFails = [System.Collections.Generic.List[string]]::new()
$processed = 0
$missing = [System.Collections.Generic.List[string]]::new()

foreach ($profile in $profileNames) {
    $matrix[$profile] = @{}
    $profileDir = Join-Path $ProfileTestDir $profile
    $profileRule = $rules.profiles.$profile
    $profileAssertionFn = $ProfileAssertions[$profile]

    foreach ($sample in $sampleNames) {
        $sampleEntry = $manifest.$sample
        $svgName = [System.IO.Path]::ChangeExtension($sample, '.svg')
        $svgPath = Join-Path $profileDir $svgName

        if (-not (Test-Path -LiteralPath $svgPath)) {
            $missing.Add("$profile/$svgName") | Out-Null
            $matrix[$profile][$sample] = $null
            continue
        }

        $svgText = Get-Content -Raw -LiteralPath $svgPath -Encoding UTF8
        $cells = [ordered] @{}

        $cellApsElements = $null
        if ($sampleEntry.PSObject.Properties['aps_elements']) {
            $cellApsElements = $sampleEntry.aps_elements
        }

        foreach ($attr in $PRESERVABLE) {
            $rule = $profileRule.attributes.$attr
            $expected = $sampleEntry.expected_output_attributes
            $sampleExpects = $false
            if ($null -ne $expected) {
                foreach ($e in $expected) { if ($e -eq $attr) { $sampleExpects = $true; break } }
            }
            $cell = Get-Cell -Rule $rule -SampleExpects $sampleExpects -SvgText $svgText -ApsElements $cellApsElements
            $cells[$attr] = $cell
            if ($cell -eq $CELL_FAIL) {
                $fails.Add("$profile | $sample | $attr") | Out-Null
            }
        }

        # Validation lanes: wellformed -> structural -> profile-specific.
        # Structural and profile checks require a parsed doc; skip both if
        # well-formedness fails.
        if ($SkipValidation) {
            $validation = [ordered] @{
                wellformed = $CELL_NA
                structural = $CELL_NA
                profile    = $CELL_NA
                errors     = @()
            }
        } else {
            $wf = Invoke-WellformednessCheck -SvgPath $svgPath
            $validation = [ordered] @{
                wellformed = if ($wf.ok) { $CELL_OK } else { $CELL_FAIL }
                structural = $CELL_NA
                profile    = $CELL_NA
                errors     = @()
            }
            if (-not $wf.ok) {
                $validation.errors = @("wellformed: $($wf.error)")
                $validationFails.Add("$profile | $sample | wellformed") | Out-Null
            } else {
                # Pass per-sample APS ground truth to enable R5 narrowing
                # (empty_by_design opt-out) and R6b (source/output type
                # consistency). When the manifest has no aps_elements for
                # this sample, $apsElements is $null -- R5 falls back to
                # strict behaviour and R6b skips.
                $apsElements = $null
                if ($sampleEntry.PSObject.Properties['aps_elements']) {
                    $apsElements = $sampleEntry.aps_elements
                }
                $struct = Invoke-StructuralChecks -Doc $wf.doc -ApsElements $apsElements
                $validation.structural = if ($struct.ok) { $CELL_OK } else { $CELL_FAIL }
                if (-not $struct.ok) {
                    $validation.errors = $struct.failures
                    $validationFails.Add("$profile | $sample | structural: $($struct.failures -join ',')") | Out-Null
                }

                if ($null -ne $profileAssertionFn) {
                    $prof = & $profileAssertionFn -Doc $wf.doc
                    $validation.profile = if ($prof.ok) { $CELL_OK } else { $CELL_FAIL }
                    if (-not $prof.ok) {
                        $validation.errors = @($validation.errors) + @($prof.failures)
                        $validationFails.Add("$profile | $sample | profile: $($prof.failures -join ',')") | Out-Null
                    }
                }
            }
        }

        $cells['_validation'] = $validation
        $matrix[$profile][$sample] = $cells
        $processed++
    }
}

# Emit _compliance-matrix.md
$mdPath = Join-Path $ProfileTestDir '_compliance-matrix.md'
$md = [System.Text.StringBuilder]::new()

[void] $md.AppendLine('# Per-Profile Compliance Matrix')
[void] $md.AppendLine()
[void] $md.AppendLine("Generated $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') by ``verify_profile_matrix.ps1``.")
[void] $md.AppendLine()
[void] $md.AppendLine('Cells:')
[void] $md.AppendLine('- `OK`   -- sample expects this attribute AND profile emits it under the spec-required name.')
[void] $md.AppendLine('- `FAIL` -- sample expects AND profile enables, but output is missing or under the wrong name.')
[void] $md.AppendLine('- `-`   -- profile enables the attribute but this sample does not carry it in source or companion XCF.')
[void] $md.AppendLine('- `n/a` -- profile disables this attribute (by spec, not by accident).')
[void] $md.AppendLine()
[void] $md.AppendLine('Validation lanes (right-most columns):')
[void] $md.AppendLine('- `wellformed` -- XML parses via System.Xml; no unclosed tags, no namespace errors.')
[void] $md.AppendLine('- `structural` -- shared SVG invariants: root in SVG namespace, viewBox present, <use> refs resolve, no empty APS groups.')
[void] $md.AppendLine('- `profile`    -- profile-specific assertions (S1000D 6.0 Table 11 / ATA iSpec 2200 §7.4.3.1). `n/a` for profiles without a published SVG output profile.')
[void] $md.AppendLine()
[void] $md.AppendLine('Source: `docs/opencgm_profile_defaults.md` sections 4.3-10.3 and 11.3.')
[void] $md.AppendLine()

# Per-profile pass criteria header.
[void] $md.AppendLine('## Pass criteria')
[void] $md.AppendLine()
foreach ($profile in $profileNames) {
    $rule = $rules.profiles.$profile
    [void] $md.AppendLine("- **$profile** -- $($rule.spec_section) -> $($rule.output_profile). Passes when no `FAIL` cell appears in its row set.")
}
[void] $md.AppendLine()

# Per-profile matrix. One table per profile keeps the column count manageable.
foreach ($profile in $profileNames) {
    [void] $md.AppendLine("## Profile: $profile")
    [void] $md.AppendLine()
    [void] $md.Append('| Sample | ')
    foreach ($attr in $PRESERVABLE) { [void] $md.Append("$attr | ") }
    [void] $md.Append('wellformed | structural | profile | ')
    [void] $md.AppendLine()
    [void] $md.Append('|---|')
    foreach ($_ in $PRESERVABLE) { [void] $md.Append('---|') }
    [void] $md.Append('---|---|---|')
    [void] $md.AppendLine()

    foreach ($sample in $sampleNames) {
        $cells = $matrix[$profile][$sample]
        if ($null -eq $cells) {
            [void] $md.AppendLine("| $sample | *output missing* |" + (' |' * ($PRESERVABLE.Count + 2)))
            continue
        }
        [void] $md.Append("| $sample | ")
        foreach ($attr in $PRESERVABLE) {
            $cell = $cells[$attr]
            [void] $md.Append("$cell | ")
        }
        $v = $cells['_validation']
        [void] $md.Append("$($v.wellformed) | $($v.structural) | $($v.profile) | ")
        [void] $md.AppendLine()
    }
    [void] $md.AppendLine()
}

# Per-profile summary.
[void] $md.AppendLine('## Summary by profile')
[void] $md.AppendLine()
[void] $md.AppendLine('| Profile | OK | FAIL | - | n/a | wellformed-FAIL | structural-FAIL | profile-FAIL | covered samples |')
[void] $md.AppendLine('|---|---|---|---|---|---|---|---|---|')
$summary = [ordered] @{}
foreach ($profile in $profileNames) {
    $ok = 0; $fail = 0; $dash = 0; $na = 0; $covered = 0
    $wfFail = 0; $structFail = 0; $profFail = 0
    foreach ($sample in $sampleNames) {
        $cells = $matrix[$profile][$sample]
        if ($null -eq $cells) { continue }
        $covered++
        foreach ($attr in $PRESERVABLE) {
            switch ($cells[$attr]) {
                $CELL_OK   { $ok++ }
                $CELL_FAIL { $fail++ }
                $CELL_DASH { $dash++ }
                $CELL_NA   { $na++ }
            }
        }
        $v = $cells['_validation']
        if ($v.wellformed -eq $CELL_FAIL) { $wfFail++ }
        if ($v.structural -eq $CELL_FAIL) { $structFail++ }
        if ($v.profile    -eq $CELL_FAIL) { $profFail++ }
    }
    [void] $md.AppendLine("| $profile | $ok | $fail | $dash | $na | $wfFail | $structFail | $profFail | $covered |")
    $summary[$profile] = [ordered] @{
        ok = $ok; fail = $fail; dash = $dash; na = $na; covered = $covered
        wellformed_fail = $wfFail; structural_fail = $structFail; profile_fail = $profFail
    }
}
[void] $md.AppendLine()

# Corpus coverage audit.
[void] $md.AppendLine('## Corpus coverage')
[void] $md.AppendLine()
$coverage = @{}
foreach ($p in $PRESERVABLE) { $coverage[$p] = 0 }
foreach ($sample in $sampleNames) {
    foreach ($attr in $manifest.$sample.expected_output_attributes) {
        if ($coverage.ContainsKey($attr)) { $coverage[$attr]++ }
    }
}
[void] $md.AppendLine('| Attribute | Covering samples | Gate (>= 3) |')
[void] $md.AppendLine('|---|---|---|')
foreach ($p in $PRESERVABLE) {
    $n = $coverage[$p]
    $status = if ($n -ge 3) { 'OK' } else { 'GAP -- author synthetic fixture' }
    [void] $md.AppendLine("| $p | $n | $status |")
}
[void] $md.AppendLine()

if ($missing.Count -gt 0) {
    [void] $md.AppendLine('## Missing outputs')
    [void] $md.AppendLine()
    foreach ($m in $missing) { [void] $md.AppendLine("- $m") }
    [void] $md.AppendLine()
}

Set-Content -LiteralPath $mdPath -Value $md.ToString() -Encoding utf8

# Emit _compliance-summary.json
$summaryPath = Join-Path $ProfileTestDir '_compliance-summary.json'
$summaryObj = [ordered] @{
    generated_at      = (Get-Date).ToString('o')
    processed_samples = $processed
    total_fails       = $fails.Count
    validation_fails  = $validationFails.Count
    missing_outputs   = @($missing)
    per_profile       = $summary
    coverage          = $coverage
    fails             = @($fails)
    validation_detail = @($validationFails)
}
$summaryObj | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $summaryPath -Encoding utf8

Write-Host "Processed: $processed (profile x sample)"
Write-Host "Attribute FAIL cells: $($fails.Count)"
Write-Host "Validation FAILs:     $($validationFails.Count)"
Write-Host "Missing outputs:      $($missing.Count)"
Write-Host "Matrix:  $mdPath"
Write-Host "Summary: $summaryPath"

if ($fails.Count -gt 0 -or $validationFails.Count -gt 0) {
    Write-Host ""
    Write-Host "Non-zero exit -- compliance failures detected." -ForegroundColor Yellow
    exit 1
}
exit 0
