# S1000D 6.0 SVG profile assertions.
#
# Source: S1000D Issue 6.0 Chapter 7.3.2 "Illustration Control Number (ICN)
# graphics" Table 11 + Chapter 4.3 preservation defaults.
#
# Invoked per sample via:
#   Invoke-S1000D6Assertions -Doc <XmlDocument>
# Returns: @{ ok = $true/$false; failures = @(<rule-id>...) }

$SVG_NS = 'http://www.w3.org/2000/svg'

function Invoke-S1000D6Assertions {
    param([System.Xml.XmlDocument] $Doc)

    $failures = [System.Collections.Generic.List[string]]::new()
    $nsm = [System.Xml.XmlNamespaceManager]::new($Doc.NameTable)
    [void] $nsm.AddNamespace('svg', $SVG_NS)

    # S1: Every element with data-aps-type="grobject" MUST carry data-apsid.
    #     Table 11 marks data-apsid normative on every grobject.
    $grobjects = $Doc.SelectNodes('//*[@data-aps-type="grobject"]')
    foreach ($g in $grobjects) {
        if (-not $g.HasAttribute('data-apsid')) {
            $failures.Add('S1:grobject-missing-apsid') | Out-Null
        }
    }

    # S2: S1000D forbids data-layer / data-aps-layer on grobjects (Chapter 4.3
    #     preserveApsLayer=false; no layer APS permitted in the S1000D profile).
    $layerOnGrobject = $Doc.SelectNodes('//*[@data-aps-type="grobject" and (@data-layer or @data-aps-layer)]')
    foreach ($n in $layerOnGrobject) {
        $failures.Add('S2:layer-attr-on-grobject') | Out-Null
    }

    # S3: aps_type values constrained to the Table 11 enumeration.
    #     grobject, layer, region, linkgrobj, link, para, text are the
    #     permitted forms; anything else is a profile violation.
    $permittedTypes = @('grobject', 'layer', 'region', 'linkgrobj', 'link', 'para', 'text')
    $typed = $Doc.SelectNodes('//*[@data-aps-type]')
    foreach ($n in $typed) {
        $v = $n.GetAttribute('data-aps-type')
        if ($permittedTypes -notcontains $v) {
            $failures.Add("S3:aps-type-unknown:$v") | Out-Null
        }
    }

    # S4: Attribute naming MUST be compact form (data-apsid / data-apsname)
    #     per S1000D 6.0 Table 11. Dashed WebCGM form (data-aps-id /
    #     data-aps-name) on a grobject is a profile drift.
    $dashedId = $Doc.SelectNodes('//*[@data-aps-type="grobject" and @data-aps-id]')
    foreach ($n in $dashedId) {
        $failures.Add('S4:grobject-dashed-apsid') | Out-Null
    }
    $dashedName = $Doc.SelectNodes('//*[@data-aps-type="grobject" and @data-aps-name]')
    foreach ($n in $dashedName) {
        $failures.Add('S4:grobject-dashed-apsname') | Out-Null
    }

    return [ordered] @{
        ok       = ($failures.Count -eq 0)
        failures = @($failures)
    }
}
