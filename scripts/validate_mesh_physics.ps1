$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$files = Get-ChildItem "$root/indra/newview/skins/default/xui/*/floater_model_preview.xml"
$english = [xml](Get-Content -LiteralPath "$root/indra/newview/skins/default/xui/en/floater_model_preview.xml" -Raw)
foreach ($file in $files) {
    $xml = [xml](Get-Content -LiteralPath $file.FullName -Raw)
    if ($xml.SelectNodes('//*[@name="Generate" or @name="mesh_upload_default_to_reliable" or @control_name="FSMeshUploadUseGLODAsDefault"]').Count) {
        throw "Obsolete GLOD control: $($file.FullName)"
    }
    foreach ($combo in $xml.SelectNodes('//combo_box[starts-with(@name,"lod_source_")]')) {
        $base = $english.SelectSingleNode("//combo_box[@name='$($combo.name)']")
        foreach ($item in $combo.SelectNodes('item')) {
            if (-not $base.SelectSingleNode("item[@name='$($item.name)']")) {
                throw "Unknown translated LOD item $($item.name): $($file.FullName)"
            }
        }
    }
}
foreach ($lod in @('high', 'medium', 'low', 'lowest')) {
    $items = @($english.SelectNodes("//combo_box[@name='lod_source_$lod']/item") | ForEach-Object { $_.name })
    $expected = @('Load from file', 'MeshOpt Auto', 'MeshOptCombine', 'MeshOptSloppy')
    if ($lod -ne 'high') {
        $expected += 'Use LoD above'
    }
    if (($items -join '|') -cne ($expected -join '|')) {
        throw "Incorrect LOD selector indices: $lod"
    }
}
foreach ($path in @('autobuild.xml', 'indra/newview/app_settings/settings.xml', 'indra/newview/skins/default/xui/en/floater_about.xml', 'indra/newview/skins/default/xui/ja/floater_about.xml')) {
    $null = [xml](Get-Content -LiteralPath "$root/$path" -Raw)
}
$activePaths = @('autobuild.xml', 'indra/cmake', 'indra/newview/CMakeLists.txt', 'indra/newview/viewer_manifest.py', 'indra/newview/llmodelpreview.cpp', 'indra/newview/llmodelpreview.h', 'indra/newview/llfloatermodelpreview.cpp', 'indra/newview/app_settings', 'indra/newview/skins')
$matches = & git -C $root grep -n -i -e glod -e mesh_upload_default_to_reliable -- @activePaths
if ($LASTEXITCODE -ne 1) {
    throw "GLOD retirement audit failed (exit $LASTEXITCODE): $matches"
}
& git -C $root diff --check
if ($LASTEXITCODE -ne 0) {
    throw 'Whitespace validation failed'
}
Write-Output "PASS: $($files.Count) uploader XML files, translated item names, four selector indices, settings/package XML, GLOD reference audit, and diff whitespace."
& git -C $root status --short --branch