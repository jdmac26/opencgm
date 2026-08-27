# Shared SVG structural assertions. Applied to every profile's output.
#
# Two lanes:
#   Invoke-WellformednessCheck -- parse via System.Xml.XmlDocument; returns
#     @{ ok = $true/$false; error = $null/<message> }.
#   Invoke-StructuralChecks   -- XPath-based invariants on the parsed doc;
#     returns @{ ok = $true/$false; failures = @(<rule id>...) }.
#
# The harness calls both for every output SVG. A document that fails
# well-formedness is not subjected to structural checks (no valid doc to
# probe). Profile-specific rules (S1000D / ATA) build on top of this file.

$SVG_NS = 'http://www.w3.org/2000/svg'
$XLINK_NS = 'http://www.w3.org/1999/xlink'

function New-SvgNamespaceManager {
    param([System.Xml.XmlDocument] $Doc)
    # Emit only the manager instance; AddNamespace returns void but PS still
    # streams any trailing expression to the pipeline, so we wrap with
    # [void] to be safe.
    $nsm = [System.Xml.XmlNamespaceManager]::new($Doc.NameTable)
    [void] $nsm.AddNamespace('svg',   $SVG_NS)
    [void] $nsm.AddNamespace('xlink', $XLINK_NS)
    ,$nsm
}

function Invoke-WellformednessCheck {
    param([string] $SvgPath)

    $result = [ordered] @{ ok = $false; error = $null; doc = $null }

    try {
        $doc = [System.Xml.XmlDocument]::new()
        # Harden loader: ignore DTD, no network resolution. The converter
        # emits no DTD but samples could in theory; we never want an XXE-style
        # side-effect from a test harness.
        $settings = [System.Xml.XmlReaderSettings]::new()
        $settings.DtdProcessing = [System.Xml.DtdProcessing]::Ignore
        $settings.XmlResolver   = $null
        $settings.CloseInput    = $true

        $reader = [System.Xml.XmlReader]::Create($SvgPath, $settings)
        try {
            $doc.Load($reader)
        } finally {
            $reader.Dispose()
        }

        $result.ok  = $true
        $result.doc = $doc
    } catch {
        $result.error = $_.Exception.Message
    }

    return $result
}

function Invoke-StructuralChecks {
    param(
        [System.Xml.XmlDocument] $Doc,
        # Optional per-sample APS ground truth from docs/corpus-manifest.json's
        # aps_elements array. Shape: @(
        #   @{ id='IsoL1'; type='grobject'; empty_by_design=$true;
        #      reason='referential_stability: ...' },
        #   @{ id='IsoL2'; type='grobject' }, ...
        # )
        # When provided, enables R5 narrowing (empty_by_design opt-out) and R6b
        # (source/output type consistency). When absent, R5 falls back to its
        # original strict behaviour (any empty hotspot grobject FAILS) and R6b
        # skips -- R6a still runs, since it only reads the output SVG.
        $ApsElements = $null
    )

    $failures = [System.Collections.Generic.List[string]]::new()
    $nsm = [System.Xml.XmlNamespaceManager]::new($Doc.NameTable)
    [void] $nsm.AddNamespace('svg',   $SVG_NS)
    [void] $nsm.AddNamespace('xlink', $XLINK_NS)

    # Build ApsElements lookup by id. Accepts either a hashtable-of-hashtables
    # or an array of PSCustomObject (the shape ConvertFrom-Json produces).
    $apsById = @{}
    if ($null -ne $ApsElements) {
        foreach ($e in $ApsElements) {
            if ($null -eq $e) { continue }
            $id = $null
            if ($e -is [hashtable])          { $id = $e['id'] }
            elseif ($e.PSObject.Properties['id']) { $id = $e.id }
            if (-not [string]::IsNullOrEmpty($id)) { $apsById[$id] = $e }
        }
    }

    # R1: Root element is <svg> in the SVG namespace.
    $root = $Doc.DocumentElement
    if ($null -eq $root) {
        $failures.Add('R1:root-missing') | Out-Null
    } else {
        if ($root.LocalName -ne 'svg')  { $failures.Add('R1:root-name')      | Out-Null }
        if ($root.NamespaceURI -ne $SVG_NS) { $failures.Add('R1:root-namespace') | Out-Null }
    }

    # R2: viewBox present on root.
    if ($null -ne $root -and -not $root.HasAttribute('viewBox')) {
        $failures.Add('R2:viewBox-missing') | Out-Null
    }

    # R3: width/height present or derivable from viewBox. The engine always
    #     emits both width+height OR a viewBox; a document with neither is
    #     unusable in viewers that rely on intrinsic sizing.
    if ($null -ne $root) {
        $hasW = $root.HasAttribute('width')
        $hasH = $root.HasAttribute('height')
        $hasVb = $root.HasAttribute('viewBox')
        if (-not $hasVb -and (-not $hasW -or -not $hasH)) {
            $failures.Add('R3:no-sizing') | Out-Null
        }
    }

    # R4: Every <use> resolves to an id defined in the document. Broken
    #     references are silent in most viewers; we catch them here.
    #     xlink:href and href are both accepted (SVG2 style).
    $uses = $Doc.SelectNodes('//svg:use', $nsm)
    if ($null -ne $uses -and $uses.Count -gt 0) {
        # Collect defined ids once.
        $definedIds = [System.Collections.Generic.HashSet[string]]::new()
        $allWithId = $Doc.SelectNodes('//*[@id]')
        foreach ($n in $allWithId) { [void] $definedIds.Add($n.GetAttribute('id')) }

        foreach ($u in $uses) {
            $href = $u.GetAttribute('href')
            if ([string]::IsNullOrEmpty($href)) {
                $href = $u.GetAttribute('href', $XLINK_NS)
            }
            if ([string]::IsNullOrEmpty($href)) {
                $failures.Add('R4:use-no-href') | Out-Null
                continue
            }
            if ($href.StartsWith('#')) {
                $target = $href.Substring(1)
                if (-not $definedIds.Contains($target)) {
                    $failures.Add("R4:use-dangling:$target") | Out-Null
                }
            }
            # External-URI <use> references (http://...) are out of scope:
            # the converter never emits them.
        }
    }

    # R5: No empty <g> whose data-aps-type is a hotspot type (grobject or
    #     linkgrobj) UNLESS the manifest explicitly marks it empty_by_design.
    #     Empty hotspot wrappers surface as ghost hitboxes in viewers, but
    #     S1000D illustration authors legitimately preserve empty grobject
    #     APS declarations for referential stability -- a companion data
    #     module's <hotspot applicationStructureIdent="..."/> may still
    #     reference the identifier even after this revision removed the
    #     callout geometry. The manifest's aps_elements[].empty_by_design
    #     flag (with a required human-readable reason) flips R5 from FAIL
    #     to PASS on that identifier.
    #
    #     Pinned definition of "empty":
    #       An element is empty when it has zero CHILD ELEMENTS, ignoring
    #       whitespace-only text nodes and XML comments. A <g> containing
    #       only <desc>metadata</desc> is NOT empty (it is a metadata-only
    #       wrapper, which R5 correctly passes).
    #
    #     Empty layer wrappers (data-aps-type="layer") are permitted
    #     unconditionally: a layer declared in CGM with no visible geometry
    #     is a valid structural construct, not a hotspot bug.
    $hotspotGroups = $Doc.SelectNodes('//svg:g[@data-aps-type="grobject" or @data-aps-type="linkgrobj"]', $nsm)
    foreach ($g in $hotspotGroups) {
        # isEmpty per pinned definition: no child elements, ignoring text
        # nodes (whitespace or otherwise) and comments.
        $hasChildElement = $false
        foreach ($c in $g.ChildNodes) {
            if ($c.NodeType -eq [System.Xml.XmlNodeType]::Element) {
                $hasChildElement = $true
                break
            }
        }
        if (-not $hasChildElement) {
            $apsid = $g.GetAttribute('data-apsid')
            if ([string]::IsNullOrEmpty($apsid)) { $apsid = $g.GetAttribute('data-aps-id') }
            if ([string]::IsNullOrEmpty($apsid)) { $apsid = $g.GetAttribute('id') }

            $exemptByDesign = $false
            if (-not [string]::IsNullOrEmpty($apsid) -and $apsById.ContainsKey($apsid)) {
                $entry = $apsById[$apsid]
                $flag = $null
                if ($entry -is [hashtable]) { $flag = $entry['empty_by_design'] }
                elseif ($entry.PSObject.Properties['empty_by_design']) { $flag = $entry.empty_by_design }
                if ($flag -eq $true) { $exemptByDesign = $true }
            }

            if (-not $exemptByDesign) {
                $failures.Add("R5:empty-hotspot-group:$apsid") | Out-Null
            }
        }
    }

    # R6a: data-aps-type values are drawn from the permitted enumeration.
    #      Defends against future regressions where the engine might emit a
    #      garbage value (typo, unmapped case, hardcoded literal left over).
    #      Enumeration derived from the APS type vocabulary actually used by
    #      the engine across S1000D / WebCGM / ATA profiles plus values
    #      reserved for APS nesting. Additions require a plan update.
    #      'grnode' added 2026-04-25 per static-bucket triage: WebCGM 2.1
    #      compound APS type used by transformAnimation and other grouping-
    #      with-transform cases; engine emission is correct.
    $permittedApsTypes = @(
        'grobject', 'layer', 'region', 'linkgrobj', 'link', 'para', 'text',
        'grnode'
    )
    $allApsTyped = $Doc.SelectNodes('//*[@data-aps-type]', $nsm)
    foreach ($n in $allApsTyped) {
        $t = $n.GetAttribute('data-aps-type')
        if ([string]::IsNullOrEmpty($t)) { continue }
        if ($permittedApsTypes -notcontains $t) {
            $nid = $n.GetAttribute('data-apsid')
            if ([string]::IsNullOrEmpty($nid)) { $nid = $n.GetAttribute('data-aps-id') }
            if ([string]::IsNullOrEmpty($nid)) { $nid = $n.GetAttribute('id') }
            $label = if ([string]::IsNullOrEmpty($nid)) { '(anonymous)' } else { $nid }
            $failures.Add("R6a:invalid-aps-type:${label}:${t}") | Out-Null
        }
    }

    # R6b: data-aps-type value matches the source APS type recorded in the
    #      manifest. Requires aps_elements ground truth; skips silently when
    #      absent. Catches regressions where the output emits a different
    #      type value than the source CGM declared -- e.g. if a hardcoded
    #      literal ever wins over a correct per-element mapping.
    if ($apsById.Count -gt 0) {
        foreach ($n in $allApsTyped) {
            $t = $n.GetAttribute('data-aps-type')
            if ([string]::IsNullOrEmpty($t)) { continue }
            $nid = $n.GetAttribute('data-apsid')
            if ([string]::IsNullOrEmpty($nid)) { $nid = $n.GetAttribute('data-aps-id') }
            if ([string]::IsNullOrEmpty($nid)) { $nid = $n.GetAttribute('id') }
            if ([string]::IsNullOrEmpty($nid)) { continue }
            if (-not $apsById.ContainsKey($nid)) { continue }

            $entry = $apsById[$nid]
            $expectedType = $null
            if ($entry -is [hashtable]) { $expectedType = $entry['type'] }
            elseif ($entry.PSObject.Properties['type']) { $expectedType = $entry.type }
            if ([string]::IsNullOrEmpty($expectedType)) { continue }

            if ($expectedType -ne $t) {
                $failures.Add("R6b:aps-type-mismatch:${nid}:expected=${expectedType},got=${t}") | Out-Null
            }
        }
    }

    return [ordered] @{
        ok       = ($failures.Count -eq 0)
        failures = @($failures)
    }
}
