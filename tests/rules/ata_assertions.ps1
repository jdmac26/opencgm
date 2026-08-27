# ATA iSpec 2200 SVG profile assertions.
#
# Source: ATA iSpec 2200 §7.4.3.1 "SVG conformance" + the opencgm defaults
# document §5.3 preservation defaults.
#
# Scope: ATA permits everything S1000D permits (attribute naming is identical:
# compact data-apsid / data-apsname form) PLUS data-layer on grobjects.
# Layer information is normative in ATA content (IPC breakdown by zone).
#
# Invoked per sample via:
#   Invoke-AtaAssertions -Doc <XmlDocument>
# Returns: @{ ok = $true/$false; failures = @(<rule-id>...) }

$SVG_NS = 'http://www.w3.org/2000/svg'

function Invoke-AtaAssertions {
    param([System.Xml.XmlDocument] $Doc)

    $failures = [System.Collections.Generic.List[string]]::new()

    # A1: Same as S1 -- every grobject carries data-apsid.
    $grobjects = $Doc.SelectNodes('//*[@data-aps-type="grobject"]')
    foreach ($g in $grobjects) {
        if (-not $g.HasAttribute('data-apsid')) {
            $failures.Add('A1:grobject-missing-apsid') | Out-Null
        }
    }

    # A2: Attribute naming MUST be compact (same as S1000D). Dashed form on
    #     a grobject is a profile drift even though ATA enables layer.
    $dashedId = $Doc.SelectNodes('//*[@data-aps-type="grobject" and @data-aps-id]')
    foreach ($n in $dashedId) {
        $failures.Add('A2:grobject-dashed-apsid') | Out-Null
    }
    $dashedName = $Doc.SelectNodes('//*[@data-aps-type="grobject" and @data-aps-name]')
    foreach ($n in $dashedName) {
        $failures.Add('A2:grobject-dashed-apsname') | Out-Null
    }

    # A3: aps_type enumeration. Same set as S1000D -- Table 11's enumeration
    #     is shared across the two profiles.
    $permittedTypes = @('grobject', 'layer', 'region', 'linkgrobj', 'link', 'para', 'text')
    $typed = $Doc.SelectNodes('//*[@data-aps-type]')
    foreach ($n in $typed) {
        $v = $n.GetAttribute('data-aps-type')
        if ($permittedTypes -notcontains $v) {
            $failures.Add("A3:aps-type-unknown:$v") | Out-Null
        }
    }

    # A4: ATA PERMITS data-layer; no assertion needed to enforce absence.
    #     (Explicit no-op rule kept as a comment so a future reader does
    #     not add an S1000D-style S2 rule here by mistake.)

    return [ordered] @{
        ok       = ($failures.Count -eq 0)
        failures = @($failures)
    }
}
