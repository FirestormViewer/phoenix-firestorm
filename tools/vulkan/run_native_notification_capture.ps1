param(
    [Parameter(Mandatory=$true)][string]$Fixture,
    [Parameter(Mandatory=$true)][string]$Request,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [switch]$Maximized,
    [ValidateSet('None','focus','pressed','1')][string]$ButtonStates='None',
    [string]$CaptureHelper=(Join-Path $PSScriptRoot '../../build-vc170-64/llvulkan/RelWithDebInfo/notification_capture.exe'),
    [switch]$HoverOnly,
    [switch]$Continuous
)
$ErrorActionPreference='Stop'
$fixturePath=(Resolve-Path -LiteralPath $Fixture).Path
$requestPath=(Resolve-Path -LiteralPath $Request).Path
$requestXml=[xml](Get-Content -LiteralPath $requestPath -Raw)
$sequenceNode=$requestXml.SelectSingleNode('/llsd/map/key[text()="sequence"]/following-sibling::string[1]')
$sequence=if ($sequenceNode) { $sequenceNode.InnerText } else { 'None' }
$queuedNode=$requestXml.SelectSingleNode('/llsd/map/key[text()="queuedInput"]/following-sibling::boolean[1]')
$queuedInput=$queuedNode -and $queuedNode.InnerText -eq 'true'
if ($sequence -notin @('None','Browser','Dialogs')) { throw 'Unsupported native capture sequence.' }
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
$process=$null
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
    if ($sequence -ne 'None') {
        $watcher=[IO.FileSystemWatcher]::new($root,'sequence-ready.txt')
        try {
            $watcher.EnableRaisingEvents=$true
            $deadline=[DateTime]::UtcNow.AddSeconds(90)
            while (!(Test-Path (Join-Path $root 'sequence-ready.txt'))) {
                if ($process.HasExited) { throw 'Native fixture exited before sequence readiness.' }
                if ([DateTime]::UtcNow -ge $deadline) { throw 'Native sequence readiness deadline exceeded.' }
                $watcher.WaitForChanged([IO.WatcherChangeTypes]::Created,1000) | Out-Null
            }
        } finally { $watcher.Dispose() }
        & (Join-Path $PSScriptRoot 'run_browser_sequence.ps1') -ViewerProcessId $process.Id -CaptureHelper $CaptureHelper -OutputDirectory $root -Backend native -HoverOnly:$HoverOnly -Dialogs:($sequence -eq 'Dialogs') -Continuous:$Continuous -QueuedInput:$queuedInput
    }
    if ($sequence -ne 'None') {
        if (!$process.WaitForExit(60000)) { throw 'Native fixture did not exit after sequence completion.' }
    } else { $process.WaitForExit() }
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
        sequence=$sequence
        queuedInput=$queuedInput
        exitCode=$process.ExitCode
    }
    $manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'manifest.json') -Encoding utf8
    if ($process.ExitCode -ne 0) { throw "Native fixture failed: $($process.ExitCode)" }
    $manifest | ConvertTo-Json
} finally {
    if ($process -and !$process.HasExited) {
        $process.CloseMainWindow() | Out-Null
        if (!$process.WaitForExit(60000)) { Write-Warning 'Native fixture still running; no forced termination performed.' }
    }
    if (!$server.HasExited) {
        $server.StandardInput.WriteLine('close')
        $server.StandardInput.Flush()
        if (!$server.WaitForExit(10000)) { Write-Warning 'Local page server did not close gracefully.' }
    }
    $server.Dispose()
}