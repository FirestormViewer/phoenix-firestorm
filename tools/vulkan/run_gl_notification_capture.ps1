param(
    [Parameter(Mandatory=$true)][string]$Viewer,
    [Parameter(Mandatory=$true)][string]$Driver,
    [Parameter(Mandatory=$true)][string]$CaptureHelper,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [switch]$PrepareOnly,
    [switch]$Maximized
)
$ErrorActionPreference='Stop'
foreach ($executable in $Viewer,$Driver,$CaptureHelper) {
    if (!(Test-Path -LiteralPath $executable -PathType Leaf)) { throw "Missing executable: $executable" }
}
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Refusing to overwrite capture evidence.' }
$root=[IO.Directory]::CreateDirectory($OutputDirectory).FullName
$pageServer=$null
try {
$pageStart=[Diagnostics.ProcessStartInfo]::new((Get-Command node -ErrorAction Stop).Source)
$pageStart.UseShellExecute=$false
$pageStart.RedirectStandardInput=$true
$pageStart.RedirectStandardOutput=$true
$pageStart.ArgumentList.Add((Join-Path $PSScriptRoot 'notification_background.cjs'))
$pageServer=[Diagnostics.Process]::Start($pageStart)
$pageReady=$pageServer.StandardOutput.ReadLineAsync()
if (!$pageReady.Wait(10000)) { throw 'Loopback page server did not become ready.' }
$pageUrl=$pageReady.Result
$pageUri=[Uri]$pageUrl
if (!$pageUri.IsLoopback -or $pageUri.Scheme -ne 'http') { throw 'Invalid fixture page URL.' }
$pageResponse=Invoke-WebRequest -Uri $pageUri -TimeoutSec 10
if ($pageResponse.StatusCode -ne 200 -or $pageResponse.Content -ne [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'notification_background.html'))) {
    throw 'Loopback fixture page did not match its source.'
}
$roaming=[IO.Directory]::CreateDirectory((Join-Path $root 'roaming')).FullName
$local=[IO.Directory]::CreateDirectory((Join-Path $root 'local')).FullName
$log=Join-Path $roaming 'Vulkanstorm_x64/logs/Vulkanstorm.log'
$watcher=[IO.FileSystemWatcher]::new($root,'gl-notification-submitted.xml')
$watcher.EnableRaisingEvents=$true
$start=[Diagnostics.ProcessStartInfo]::new((Resolve-Path $Viewer).Path)
$start.UseShellExecute=$false
$start.WorkingDirectory=Split-Path $start.FileName
$start.Environment['APPDATA']=$roaming
$start.Environment['LOCALAPPDATA']=$local
$start.Environment.Remove('VULKANSTORM_CAPTURE') | Out-Null
$start.Environment.Remove('VULKANSTORM_UITEST') | Out-Null
$settings=[ordered]@{
    RenderBackend=@('String','string','OpenGL'); AutoLogin=@('Boolean','boolean','false')
    NoAudio=@('Boolean','boolean','true'); EnableVoiceChat=@('Boolean','boolean','false')
    FirstRunThisInstall=@('Boolean','boolean','false'); WindowMaximized=@('Boolean','boolean',$Maximized.IsPresent.ToString().ToLowerInvariant())
    WindowWidth=@('S32','integer','1024'); WindowHeight=@('S32','integer','738')
    Language=@('String','string','en'); SkinCurrent=@('String','string','default')
    SkinCurrentTheme=@('String','string','default'); UIScaleFactor=@('F32','real','1.0')
    LoginPage=@('String','string','about:blank')
    ForceLoginURL=@('String','string',$pageUrl)
    FSShowWhitelistReminder=@('Boolean','boolean','false')
    LeapCommand=@('LLSD','array',('"'+(Resolve-Path $Driver).Path.Replace('\','/')+'" "'+$root.Replace('\','/')+'"'))
}
$userSettings=[IO.Directory]::CreateDirectory((Join-Path $roaming 'Vulkanstorm_x64/user_settings')).FullName
$settingsName='fsdata_defaults.7.2.5.xml'
$settingsPath=Join-Path $userSettings $settingsName
$defaultsPath=Join-Path $start.WorkingDirectory 'app_settings/settings.xml'
$defaults=[xml](Get-Content -LiteralPath $defaultsPath -Raw)
foreach ($entry in $settings.GetEnumerator()) {
    $declaration=$defaults.SelectSingleNode("/llsd/map/key[text()='$($entry.Key)']/following-sibling::map[1]")
    if (!$declaration) { throw "Reference has no setting: $($entry.Key)" }
    $type=$declaration.SelectSingleNode("key[text()='Type']/following-sibling::string[1]").InnerText
    $valueNode=$declaration.SelectSingleNode("key[text()='Value']/following-sibling::*[1]")
    if ($type -eq 'Boolean' -and $valueNode.LocalName -eq 'integer' -and $entry.Value[1] -eq 'boolean') {
        $entry.Value[1]='integer'
        $entry.Value[2]=if ([bool]::Parse($entry.Value[2])) { '1' } else { '0' }
    }
    if ($valueNode.LocalName -ne $entry.Value[1]) { throw "Fixture value encoding differs from reference: $($entry.Key)" }
    $entry.Value[0]=$type
}
$writer=[Xml.XmlWriter]::Create($settingsPath)
try {
    $writer.WriteStartElement('llsd'); $writer.WriteStartElement('map')
    foreach ($entry in $settings.GetEnumerator()) {
        $writer.WriteElementString('key',$entry.Key); $writer.WriteStartElement('map')
        $writer.WriteElementString('key','Comment'); $writer.WriteElementString('string','Isolated notification capture fixture')
        $persist=$defaults.SelectSingleNode("/llsd/map/key[text()='$($entry.Key)']/following-sibling::map[1]/key[text()='Persist']/following-sibling::integer[1]")
        if ($persist) { $writer.WriteElementString('key','Persist'); $writer.WriteElementString('integer',$persist.InnerText) }
        $writer.WriteElementString('key','Type'); $writer.WriteElementString('string',$entry.Value[0])
        $writer.WriteElementString('key','Value')
        if ($entry.Value[1] -eq 'array') {
            $writer.WriteStartElement('array'); $writer.WriteElementString('string',$entry.Value[2]); $writer.WriteEndElement()
        } else { $writer.WriteElementString($entry.Value[1],$entry.Value[2]) }
        $writer.WriteEndElement()
    }
    $writer.WriteEndElement(); $writer.WriteEndElement()
} finally { $writer.Dispose() }
& $Driver --check-settings $defaultsPath $settingsPath
if ($LASTEXITCODE -ne 0) { throw 'Fixture defaults preflight failed.' }
foreach ($argument in @('--noaudio','--novoice','--skipupdatecheck')) {
    $start.ArgumentList.Add($argument)
}
if ($PrepareOnly) {
    $document=[xml](Get-Content -LiteralPath $settingsPath -Raw)
    if (!$document.SelectSingleNode('/llsd/map/key[text()="RenderBackend"]/following-sibling::map[1]/string[text()="OpenGL"]')) {
        throw 'Prepared settings lack the OpenGL backend selection.'
    }
    $watcher.Dispose()
    [pscustomobject]@{Settings=$settingsPath; Arguments=@($start.ArgumentList); LaunchPerformed=$false}
    return
}
$process=$null
try {
    $process=[Diagnostics.Process]::Start($start)
    $submission=Join-Path $root 'gl-notification-submitted.xml'
    $deadline=[DateTime]::UtcNow.AddMinutes(3)
    while (!(Test-Path -LiteralPath $submission)) {
        if ($process.HasExited) { throw "Reference exited before notification: $($process.ExitCode)" }
        $remaining=[int]($deadline-[DateTime]::UtcNow).TotalMilliseconds
        if ($remaining -le 0) { throw 'No notification submission within three minutes.' }
        $watcher.WaitForChanged([IO.WatcherChangeTypes]::Created,[Math]::Min($remaining,1000)) | Out-Null
    }
    $process.Refresh()
    $window=$process.MainWindowHandle
    if (!$window) { throw 'Reference has no main window.' }
    if (-not ('NotificationCaptureWindowState' -as [type])) { Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class NotificationCaptureWindowState {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr window);
    [DllImport("user32.dll")] public static extern bool IsZoomed(IntPtr window);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window,uint message,IntPtr parameter,IntPtr data);
}
'@
    }
    [NotificationCaptureWindowState]::SetForegroundWindow($window) | Out-Null
    $actualMaximized=[NotificationCaptureWindowState]::IsZoomed($window)
    if ($Maximized -and !$actualMaximized) { throw 'Reference window is not maximized; refusing mismatched capture.' }
    $settledAfter=[DateTime]::UtcNow.AddSeconds(2)
    $index=0
    do {
        $file=Join-Path $root ("gl-MediaPluginFailed-$index.rgba")
        & $CaptureHelper $window.ToInt64().ToString() $file
        if ($LASTEXITCODE -ne 0) { throw 'Windows Graphics Capture failed.' }
        ++$index
    } while ([DateTime]::UtcNow -lt $settledAfter -and $index -lt 30)
    foreach ($state in 0,1) {
        & $CaptureHelper $window.ToInt64().ToString() (Join-Path $root "gl-MediaPluginFailed-settled-$state.rgba")
        if ($LASTEXITCODE -ne 0) { throw 'Settled capture failed.' }
    }
    [NotificationCaptureWindowState]::PostMessage($window,0x100,[IntPtr]13,[IntPtr]1) | Out-Null
    [NotificationCaptureWindowState]::PostMessage($window,0x101,[IntPtr]13,[IntPtr]1) | Out-Null
    $process.CloseMainWindow() | Out-Null
    if (!$process.WaitForExit(60000)) { throw 'Reference did not close within 60 seconds; no forced termination performed.' }
    $goodbye=(Test-Path $log) -and (Select-String -LiteralPath $log -SimpleMatch 'Goodbye!' -Quiet)
    $manifest=[ordered]@{
        fixture='pre-login local notification; not STATE_STARTED acceptance'
        referenceRevision='59108e15a1f8f94d2da7c674d937d19f5cf9450d'
        viewer=$start.FileName; viewerSha256=(Get-FileHash $Viewer).Hash
        driverSha256=(Get-FileHash $Driver).Hash; captureSha256=(Get-FileHash $CaptureHelper).Hash
        backgroundUrl=$pageUrl; backgroundSha256=(Get-FileHash (Join-Path $PSScriptRoot 'notification_background.html')).Hash
        notification='MediaPluginFailed'; plugin='media_plugin_cef'; locale='en'
        requestedMaximized=$Maximized.IsPresent; actualMaximized=$actualMaximized
        pid=$process.Id; exitCode=$process.ExitCode; goodbye=$goodbye
        responseRecorded=(Test-Path (Join-Path $root 'gl-notification-response.xml'))
    }
    $manifest | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'manifest.json') -Encoding utf8
    if ($process.ExitCode -ne 0 -or !$goodbye -or !$manifest.responseRecorded) { throw 'Reference response or exit evidence failed.' }
    $manifest | ConvertTo-Json
} finally {
    $watcher.Dispose()
    if ($process -and !$process.HasExited) {
        $process.CloseMainWindow() | Out-Null
        if (!$process.WaitForExit(60000)) { Write-Warning "Reference PID $($process.Id) remains running; close gracefully. No process was killed." }
    }
}
} finally {
    if ($pageServer -and !$pageServer.HasExited) {
        $pageServer.StandardInput.WriteLine('close')
        $pageServer.StandardInput.Flush()
        if (!$pageServer.WaitForExit(10000)) { Write-Warning 'Loopback page server did not finish graceful shutdown.' }
    }
    if ($pageServer) { $pageServer.Dispose() }
}