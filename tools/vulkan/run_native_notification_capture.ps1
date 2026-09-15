param(
    [Parameter(Mandatory=$true)][string]$Fixture,
    [Parameter(Mandatory=$true)][string]$Request,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [switch]$Maximized,
    [ValidateSet('None','focus','pressed','1')][string]$ButtonStates='None'
)
$ErrorActionPreference='Stop'
$fixturePath=(Resolve-Path -LiteralPath $Fixture).Path
$requestPath=(Resolve-Path -LiteralPath $Request).Path
$requestXml=[xml](Get-Content -LiteralPath $requestPath -Raw)
$pageNode=$requestXml.SelectSingleNode('/llsd/map/key[text()="pagePath"]/following-sibling::string[1]')
$pagePath=if ($pageNode) { (Resolve-Path -LiteralPath $pageNode.InnerText).Path } else { Join-Path $PSScriptRoot 'notification_background.html' }
$pageHash=(Get-FileHash -LiteralPath $pagePath).Hash
if ($pageNode) {
    $expectedHash=$requestXml.SelectSingleNode('/llsd/map/key[text()="pageSha256"]/following-sibling::string[1]')
    if (!$expectedHash -or $expectedHash.InnerText -ne $pageHash) { throw 'Native fixture page differs from reference request.' }
}
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Refusing to overwrite capture evidence.' }
$root=[IO.Directory]::CreateDirectory($OutputDirectory).FullName
$serverStart=[Diagnostics.ProcessStartInfo]::new((Get-Command node -ErrorAction Stop).Source)
$serverStart.UseShellExecute=$false
$serverStart.RedirectStandardInput=$true
$serverStart.RedirectStandardOutput=$true
$serverStart.ArgumentList.Add((Join-Path $PSScriptRoot 'notification_background.cjs'))
$serverStart.ArgumentList.Add($pagePath)
$server=[Diagnostics.Process]::Start($serverStart)
try {
    $ready=$server.StandardOutput.ReadLineAsync()
    if (!$ready.Wait(10000)) { throw 'Local page server readiness timeout.' }
    $pageUrl=$ready.Result
    $pageUri=[Uri]$pageUrl
    if (!$pageUri.IsLoopback -or $pageUri.Scheme -ne 'http') { throw 'Invalid local page URL.' }
    $start=[Diagnostics.ProcessStartInfo]::new($fixturePath)
    $start.UseShellExecute=$false
    $start.WorkingDirectory=Split-Path $fixturePath
    $start.Environment['LLVK_NOTIFICATION_CAPTURE_DIR']=$root
    $start.Environment['LLVK_NOTIFICATION_CAPTURE_PAGE']=$pageUrl
    $start.Environment['LLVK_NOTIFICATION_CAPTURE_REQUEST']=$requestPath
    $start.Environment.Remove('LLVK_NOTIFICATION_CAPTURE_MAXIMIZED') | Out-Null
    $start.Environment.Remove('LLVK_CAPTURE_LOGIN_BUTTON_STATES') | Out-Null
    if ($Maximized) { $start.Environment['LLVK_NOTIFICATION_CAPTURE_MAXIMIZED']='1' }
    if ($ButtonStates -ne 'None') { $start.Environment['LLVK_CAPTURE_LOGIN_BUTTON_STATES']=$ButtonStates }
    $start.Environment['VK_LOADER_LAYERS_DISABLE']='VK_LAYER_LUNARG_api_dump'
    $process=[Diagnostics.Process]::Start($start)
    $process.WaitForExit()
    $manifest=[ordered]@{
        fixture='isolated pre-login component; no authentication'
        executable=$fixturePath
        executableSha256=(Get-FileHash -LiteralPath $fixturePath).Hash
        request=$requestPath
        requestSha256=(Get-FileHash -LiteralPath $requestPath).Hash
        backgroundUrl=$pageUrl
        backgroundSha256=$pageHash
        backgroundPath=$pagePath
        requestedMaximized=$Maximized.IsPresent
        buttonStates=$ButtonStates
        exitCode=$process.ExitCode
    }
    $manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'manifest.json') -Encoding utf8
    if ($process.ExitCode -ne 0) { throw "Native fixture failed: $($process.ExitCode)" }
    $manifest | ConvertTo-Json
} finally {
    if (!$server.HasExited) {
        $server.StandardInput.WriteLine('close')
        $server.StandardInput.Flush()
        if (!$server.WaitForExit(10000)) { Write-Warning 'Local page server did not close gracefully.' }
    }
    $server.Dispose()
}