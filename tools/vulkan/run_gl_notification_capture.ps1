param(
    [Parameter(Mandatory=$true)][string]$Viewer,
    [Parameter(Mandatory=$true)][string]$Driver,
    [Parameter(Mandatory=$true)][string]$CaptureHelper,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [ValidatePattern('^[0-9a-f]{40}(\+working-tree)?$')][string]$ReferenceRevision='59108e15a1f8f94d2da7c674d937d19f5cf9450d',
    [switch]$PrepareOnly,
    [switch]$Maximized,
    [switch]$LoginButtonStates,
    [ValidatePattern('^[A-Za-z][A-Za-z0-9]*$')][string]$Notification='MediaPluginFailed',
    [hashtable]$Substitutions=@{PLUGIN='media_plugin_cef'},
    [string]$InputText='',
    [switch]$SelectInput,
    [switch]$CheckIgnore,
    [switch]$InactiveFocus,
    [ValidateSet('None','Browser','Dialogs')][string]$Sequence='None',
    [switch]$Continuous,
    [switch]$TearOff,
    [switch]$TearOffLifecycle,
    [switch]$HelpBrowser,
    [switch]$DialogLifecycle,
    [switch]$DialogMovement,
    [switch]$ControlledReplay,
    [switch]$QueuedInput,
    [string]$Page=(Join-Path $PSScriptRoot 'notification_background.html'),
    [ValidatePattern('^[a-z]{2}(-[A-Z]{2})?$')][string]$Language='en',
    [ValidateRange(640,7680)][int]$Width=1024,
    [ValidateRange(480,4320)][int]$Height=738,
    [ValidateRange(0.75,2.0)][double]$UiScale=1.0,
    [ValidateSet('enabled','hover','pressed','focus-lost','focus-regained')][string[]]$ButtonStates=@('enabled','hover','pressed'),
    [ValidateSet('Unchanged','Off','On')][string]$Anisotropy='Unchanged'
)
$ErrorActionPreference='Stop'
if ($ControlledReplay -and (!$TearOff -or !$QueuedInput -or $ReferenceRevision -notlike '*+working-tree')) { throw 'Controlled replay requires an explicitly identified diagnostic binary and queued tear-off input.' }
if ($TearOff -and $Sequence -ne 'Dialogs') { throw 'Tear-off capture requires the dialog sequence.' }
if ($TearOffLifecycle -and !$TearOff) { throw 'Tear-off lifecycle requires tear-off capture.' }
if ($HelpBrowser -and ($Sequence -ne 'Dialogs' -or $TearOff)) { throw 'Help browser capture requires a separate dialog sequence.' }
if ($DialogLifecycle -and ($Sequence -ne 'Dialogs' -or $TearOff)) { throw 'Dialog lifecycle requires a non-tear-off dialog sequence.' }
if ($DialogMovement -and !$DialogLifecycle) { throw 'Dialog movement requires the dialog lifecycle.' }
$pagePath=(Resolve-Path -LiteralPath $Page).Path
$pageHash=(Get-FileHash -LiteralPath $pagePath).Hash
if ($Sequence -ne 'None' -and (!$Maximized -or ($UiScale -ne 1 -and $Sequence -ne 'Dialogs') -or $LoginButtonStates -or $InactiveFocus)) {
    throw 'Browser sequence requires maximized 100-percent active capture without login-button states.'
}
foreach ($executable in $Viewer,$Driver,$CaptureHelper) {
    if (!(Test-Path -LiteralPath $executable -PathType Leaf)) { throw "Missing executable: $executable" }
}
$viewerHash=(Get-FileHash -LiteralPath $Viewer).Hash
if ($ReferenceRevision -eq '59108e15a1f8f94d2da7c674d937d19f5cf9450d' -and
    $viewerHash -ne 'CB44347B3AC03B36F94794FEF09182C4884F3A332F98692DB2C14016802C6FDF') {
    throw 'Non-oracle executable requires an explicit diagnostic ReferenceRevision.'
}
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Refusing to overwrite capture evidence.' }
$root=[IO.Directory]::CreateDirectory($OutputDirectory).FullName
$requestWriter=[Xml.XmlWriter]::Create((Join-Path $root 'capture-request.xml'))
try {
    $requestWriter.WriteStartElement('llsd'); $requestWriter.WriteStartElement('map')
    $requestWriter.WriteElementString('key','name'); $requestWriter.WriteElementString('string',$Notification)
    $requestWriter.WriteElementString('key','pagePath'); $requestWriter.WriteElementString('string',$pagePath)
    $requestWriter.WriteElementString('key','pageSha256'); $requestWriter.WriteElementString('string',$pageHash)
    if ($Sequence -ne 'None') { $requestWriter.WriteElementString('key','sequence'); $requestWriter.WriteElementString('string',$Sequence) }
    if ($QueuedInput) { $requestWriter.WriteElementString('key','queuedInput'); $requestWriter.WriteElementString('boolean','true') }
    if ($TearOff) { $requestWriter.WriteElementString('key','tearOff'); $requestWriter.WriteElementString('boolean','true') }
    if ($TearOffLifecycle) { $requestWriter.WriteElementString('key','tearOffLifecycle'); $requestWriter.WriteElementString('boolean','true') }
    if ($TearOffLifecycle) { $requestWriter.WriteElementString('key','lifecycleRevision'); $requestWriter.WriteElementString('integer','2') }
    if ($HelpBrowser) { $requestWriter.WriteElementString('key','helpBrowser'); $requestWriter.WriteElementString('boolean','true') }
    if ($DialogLifecycle) { $requestWriter.WriteElementString('key','dialogLifecycle'); $requestWriter.WriteElementString('boolean','true') }
    if ($DialogMovement) { $requestWriter.WriteElementString('key','dialogMovement'); $requestWriter.WriteElementString('boolean','true') }
    if ($ControlledReplay) { $requestWriter.WriteElementString('key','controlledReplay'); $requestWriter.WriteElementString('boolean','true') }
    $requestWriter.WriteElementString('key','substitutions'); $requestWriter.WriteStartElement('map')
    foreach ($key in ($Substitutions.Keys | Sort-Object)) {
        $requestWriter.WriteElementString('key',[string]$key); $requestWriter.WriteElementString('string',[string]$Substitutions[$key])
    }
    $requestWriter.WriteEndElement()
    if ($InputText) { $requestWriter.WriteElementString('key','input'); $requestWriter.WriteElementString('string',$InputText) }
    if ($SelectInput) { $requestWriter.WriteElementString('key','select'); $requestWriter.WriteElementString('boolean','true') }
    if ($CheckIgnore) { $requestWriter.WriteElementString('key','checkIgnore'); $requestWriter.WriteElementString('boolean','true') }
    if ($InactiveFocus) { $requestWriter.WriteElementString('key','inactiveFocus'); $requestWriter.WriteElementString('boolean','true') }
    $requestWriter.WriteElementString('key','display'); $requestWriter.WriteStartElement('map')
    $requestWriter.WriteElementString('key','Language'); $requestWriter.WriteElementString('string',$Language)
    $requestWriter.WriteElementString('key','WindowWidth'); $requestWriter.WriteElementString('integer',[string]$Width)
    $requestWriter.WriteElementString('key','WindowHeight'); $requestWriter.WriteElementString('integer',[string]$Height)
    $requestWriter.WriteElementString('key','UIScaleFactor'); $requestWriter.WriteElementString('real',$UiScale.ToString([Globalization.CultureInfo]::InvariantCulture))
    if ($Anisotropy -ne 'Unchanged') {
        $requestWriter.WriteElementString('key','RenderAnisotropic'); $requestWriter.WriteElementString('boolean',($Anisotropy -eq 'On').ToString().ToLowerInvariant())
    }
    $requestWriter.WriteEndElement()
    $requestWriter.WriteEndElement(); $requestWriter.WriteEndElement()
} finally { $requestWriter.Dispose() }
$pageServer=$null
try {
$pageStart=[Diagnostics.ProcessStartInfo]::new((Get-Command node -ErrorAction Stop).Source)
$pageStart.UseShellExecute=$false
$pageStart.RedirectStandardInput=$true
$pageStart.RedirectStandardOutput=$true
$pageStart.ArgumentList.Add((Join-Path $PSScriptRoot 'notification_background.cjs'))
$pageStart.ArgumentList.Add($pagePath)
$pageServer=[Diagnostics.Process]::Start($pageStart)
$pageReady=$pageServer.StandardOutput.ReadLineAsync()
if (!$pageReady.Wait(10000)) { throw 'Loopback page server did not become ready.' }
$pageUrl=$pageReady.Result
$pageUri=[Uri]$pageUrl
if (!$pageUri.IsLoopback -or $pageUri.Scheme -ne 'http') { throw 'Invalid fixture page URL.' }
$pageResponse=Invoke-WebRequest -Uri $pageUri -TimeoutSec 10
if ($pageResponse.StatusCode -ne 200 -or $pageResponse.Content -ne [IO.File]::ReadAllText($pagePath)) {
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
$start.Environment.Remove('LL_DIAGNOSTIC_REPLAY_DIR') | Out-Null
if ($ControlledReplay) { $start.Environment['LL_DIAGNOSTIC_REPLAY_DIR']=[IO.Directory]::CreateDirectory((Join-Path $root 'replay')).FullName }
$start.Environment.Remove('VULKANSTORM_CAPTURE') | Out-Null
$start.Environment.Remove('VULKANSTORM_UITEST') | Out-Null
$start.Environment.Remove('LLVK_CAPTURE_ANISOTROPY') | Out-Null
$start.Environment.Remove('LLVK_CAPTURE_LOGIN_BUTTON_STATES') | Out-Null
if ($LoginButtonStates) { $start.Environment['LLVK_CAPTURE_LOGIN_BUTTON_STATES']='1' }
if ($Anisotropy -ne 'Unchanged') { $start.Environment['LLVK_CAPTURE_ANISOTROPY']=if ($Anisotropy -eq 'On') { '1' } else { '0' } }
$settings=[ordered]@{
    RenderBackend=@('String','string','OpenGL'); AutoLogin=@('Boolean','boolean','false')
    NoAudio=@('Boolean','boolean','true'); EnableVoiceChat=@('Boolean','boolean','false')
    FirstRunThisInstall=@('Boolean','boolean','false'); WindowMaximized=@('Boolean','boolean',$Maximized.IsPresent.ToString().ToLowerInvariant())
    WindowWidth=@('S32','integer',[string]$Width); WindowHeight=@('S32','integer',[string]$Height)
    Language=@('String','string',$Language); SkinCurrent=@('String','string','default')
    SkinCurrentTheme=@('String','string','default'); UIScaleFactor=@('F32','real',$UiScale.ToString([Globalization.CultureInfo]::InvariantCulture))
    LoginPage=@('String','string','about:blank')
    ForceLoginURL=@('String','string',$pageUrl)
    FSShowWhitelistReminder=@('Boolean','boolean','false')
    LeapCommand=@('LLSD','array',('"'+(Resolve-Path $Driver).Path.Replace('\','/')+'" "'+$root.Replace('\','/')+'"'))
}
$userSettings=[IO.Directory]::CreateDirectory((Join-Path $roaming 'Vulkanstorm_x64/user_settings')).FullName
if ($HelpBrowser) {
    $settings['HelpURLFormat']=@('String','string',$pageUrl)
    $settings['PreferredBrowserBehavior']=@('U32','integer','2')
}
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
        if ((Test-Path -LiteralPath $log) -and (Select-String -LiteralPath $log -Pattern 'notification_leap.exe.*exited with code [1-9]' -Quiet)) {
            throw 'Reference capture driver failed; retained log contains the diagnostic.'
        }
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
    if (!$Maximized) {
        if (-not ('NotificationClientSize' -as [type])) { Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class NotificationClientSize {
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int left, top, right, bottom; }
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr window,int command);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr window,out Rect rectangle);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window,out Rect rectangle);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr window,IntPtr after,int x,int y,int width,int height,uint flags);
}
'@
        }
        [NotificationClientSize]::ShowWindow($window,9) | Out-Null
        $client=[NotificationClientSize+Rect]::new(); $outer=[NotificationClientSize+Rect]::new()
        if (![NotificationClientSize]::GetClientRect($window,[ref]$client) -or ![NotificationClientSize]::GetWindowRect($window,[ref]$outer)) { throw 'Cannot query restored reference geometry.' }
        if (![NotificationClientSize]::SetWindowPos($window,[IntPtr]::Zero,0,0,$Width+$outer.right-$outer.left-$client.right,$Height+$outer.bottom-$outer.top-$client.bottom,6)) { throw 'Cannot resize reference client.' }
        if (![NotificationClientSize]::GetClientRect($window,[ref]$client) -or $client.right -ne $Width -or $client.bottom -ne $Height) { throw 'Reference client dimensions do not match request.' }
    }
    $actualMaximized=[NotificationCaptureWindowState]::IsZoomed($window)
    if ($Maximized.IsPresent -ne $actualMaximized) { throw 'Reference maximization state differs from request.' }
    if ($InactiveFocus) {
        if (-not ('NotificationFocusState' -as [type])) { Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class NotificationFocusState {
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr window,uint message,IntPtr parameter,IntPtr data);
}
'@
        }
        if ([NotificationFocusState]::GetForegroundWindow() -eq $window) { throw 'Inactive fixture requires a nonforeground reference window.' }
        [NotificationFocusState]::SendMessage($window,0x8,[IntPtr]::Zero,[IntPtr]::Zero) | Out-Null
    }
    $pageDeadline=[DateTime]::UtcNow.AddSeconds(30)
    $pageFrame=0
    do {
        $file=Join-Path $root ("gl-page-ready-$pageFrame.rgba")
        & $CaptureHelper $window.ToInt64().ToString() $file
        if ($LASTEXITCODE -ne 0) { throw 'Browser readiness capture failed.' }
        $pixels=[IO.File]::ReadAllBytes($file)
        if ($pixels.Length -lt 8) { throw 'Browser readiness capture has no dimensions.' }
        $frameWidth=[BitConverter]::ToUInt32($pixels,0)
        $frameHeight=[BitConverter]::ToUInt32($pixels,4)
        if (!$frameWidth -or !$frameHeight -or $pixels.LongLength -ne 8+4L*$frameWidth*$frameHeight) {
            throw 'Browser readiness capture has invalid dimensions.'
        }
        $offset=8+4L*([Math]::Floor($frameHeight/4)*$frameWidth+[Math]::Floor($frameWidth/2))
        $painted=$pixels[$offset] -eq 41 -and $pixels[$offset+1] -eq 41 -and $pixels[$offset+2] -eq 41
        ++$pageFrame
    } while (!$painted -and [DateTime]::UtcNow -lt $pageDeadline)
    if (!$painted) { throw 'Fixture page did not paint its expected gray background within thirty seconds.' }
    $settledAfter=[DateTime]::UtcNow.AddSeconds(2)
    $index=0
    do {
        $file=Join-Path $root ("gl-$Notification-$index.rgba")
        & $CaptureHelper $window.ToInt64().ToString() $file
        if ($LASTEXITCODE -ne 0) { throw 'Windows Graphics Capture failed.' }
        ++$index
    } while ([DateTime]::UtcNow -lt $settledAfter -and $index -lt 30)
    foreach ($state in 0,1) {
        & $CaptureHelper $window.ToInt64().ToString() (Join-Path $root "gl-$Notification-settled-$state.rgba")
        if ($LASTEXITCODE -ne 0) { throw 'Settled capture failed.' }
        if ($InactiveFocus) {
            $observed=Get-Content (Join-Path $root "gl-$Notification-settled-$state.rgba.window.txt") -Raw
            if ($observed -notmatch 'foregroundBefore=0' -or $observed -notmatch 'foregroundAfter=0') { throw 'Reference activation changed during inactive capture.' }
        }
    }
    [NotificationCaptureWindowState]::PostMessage($window,0x100,[IntPtr]13,[IntPtr]1) | Out-Null
    [NotificationCaptureWindowState]::PostMessage($window,0x101,[IntPtr]13,[IntPtr]1) | Out-Null
    if ($LoginButtonStates) {
        if (-not ('NotificationButtonInput' -as [type])) { Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class NotificationButtonInput {
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr window,uint message,IntPtr parameter,IntPtr data);
}
'@
        }
        $header=[IO.File]::ReadAllBytes((Join-Path $root "gl-$Notification-settled-0.rgba"))
        if ([BitConverter]::ToUInt32($header,0) -ne 2560 -or [BitConverter]::ToUInt32($header,4) -ne 1369) {
            throw 'Login button input fixture requires the verified 2560x1369 client layout.'
        }
        $responseWatcher=[IO.FileSystemWatcher]::new($root,'gl-login-ready.xml')
        try {
            $responseWatcher.EnableRaisingEvents=$true
            if (!(Test-Path (Join-Path $root 'gl-login-ready.xml'))) {
                $responseWatcher.WaitForChanged([IO.WatcherChangeTypes]::Created,10000) | Out-Null
            }
            if (!(Test-Path (Join-Path $root 'gl-login-ready.xml'))) { throw 'Login was not unobstructed before login-state input.' }
        } finally { $responseWatcher.Dispose() }
        $point={param([int]$horizontal,[int]$vertical) [IntPtr](($vertical -shl 16) -bor $horizontal)}
        $outside=& $point 1280 900
        if (-not ('NotificationCursorInput' -as [type])) { Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class NotificationCursorInput {
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int x, y; }
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out Point point);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr window,ref Point point);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
}
'@
        }
        $savedCursor=[NotificationCursorInput+Point]::new()
        [NotificationCursorInput]::GetCursorPos([ref]$savedCursor) | Out-Null
        try {
            foreach ($buttonState in $ButtonStates) {
                $cursor=[NotificationCursorInput+Point]::new()
                $cursor.x=if ($buttonState -eq 'enabled') { 1280 } else { 1695 }
                $cursor.y=if ($buttonState -eq 'enabled') { 900 } else { 1266 }
                [NotificationCursorInput]::ClientToScreen($window,[ref]$cursor) | Out-Null
                [NotificationCursorInput]::SetCursorPos($cursor.x,$cursor.y) | Out-Null
                $position=if ($buttonState -eq 'enabled') { $outside } else { & $point 1695 1266 }
                [NotificationButtonInput]::SendMessage($window,0x200,[IntPtr]0,$position) | Out-Null
                if ($buttonState -eq 'pressed') {
                    [NotificationButtonInput]::SendMessage($window,0x201,[IntPtr]1,$position) | Out-Null
                }
                if ($buttonState -like 'focus-*') {
                    [NotificationButtonInput]::SendMessage($window,0x201,[IntPtr]1,$position) | Out-Null
                    [NotificationButtonInput]::SendMessage($window,0x200,[IntPtr]1,$outside) | Out-Null
                    [NotificationButtonInput]::SendMessage($window,0x202,[IntPtr]0,$outside) | Out-Null
                    [NotificationButtonInput]::SendMessage($window,0x8,[IntPtr]0,[IntPtr]0) | Out-Null
                    if ($buttonState -eq 'focus-regained') {
                        [NotificationButtonInput]::SendMessage($window,0x7,[IntPtr]0,[IntPtr]0) | Out-Null
                    }
                }
                $settle=[DateTime]::UtcNow.AddSeconds(2)
                $frame=0
                do {
                    & $CaptureHelper $window.ToInt64().ToString() (Join-Path $root "gl-login-$buttonState-transition-$frame.rgba")
                    if ($LASTEXITCODE -ne 0) { throw 'Login-state capture failed.' }
                    ++$frame
                } while ([DateTime]::UtcNow -lt $settle)
                foreach ($sample in 0,1) {
                    & $CaptureHelper $window.ToInt64().ToString() (Join-Path $root "gl-login-$buttonState-settled-$sample.rgba")
                    if ($LASTEXITCODE -ne 0) { throw 'Login-state settled capture failed.' }
                }
            }
        } finally {
            [NotificationButtonInput]::SendMessage($window,0x200,[IntPtr]1,$outside) | Out-Null
            [NotificationButtonInput]::SendMessage($window,0x202,[IntPtr]0,$outside) | Out-Null
            [NotificationCursorInput]::SetCursorPos($savedCursor.x,$savedCursor.y) | Out-Null
        }
    }
    if ($Sequence -ne 'None') {
        $responseWatcher=[IO.FileSystemWatcher]::new($root,'gl-notification-response.xml')
        try {
            $responseWatcher.EnableRaisingEvents=$true
            if (!(Test-Path (Join-Path $root 'gl-notification-response.xml'))) {
                $responseWatcher.WaitForChanged([IO.WatcherChangeTypes]::Created,10000) | Out-Null
            }
            if (!(Test-Path (Join-Path $root 'gl-notification-response.xml'))) { throw 'Sequence requires modal response.' }
        } finally { $responseWatcher.Dispose() }
        & (Join-Path $PSScriptRoot 'run_browser_sequence.ps1') -ViewerProcessId $process.Id -CaptureHelper $CaptureHelper -OutputDirectory $root -Backend gl -UiScale $UiScale -Dialogs:($Sequence -eq 'Dialogs') -TearOff:$TearOff -TearOffLifecycle:$TearOffLifecycle -HelpBrowser:$HelpBrowser -DialogLifecycle:$DialogLifecycle -DialogMovement:$DialogMovement -ControlledReplay:$ControlledReplay -Continuous:$Continuous -QueuedInput:$QueuedInput
    }
    $process.CloseMainWindow() | Out-Null
    if (!$process.WaitForExit(60000)) { throw 'Reference did not close within 60 seconds; no forced termination performed.' }
    $goodbye=(Test-Path $log) -and (Select-String -LiteralPath $log -SimpleMatch 'Goodbye!' -Quiet)
    $manifest=[ordered]@{
        queuedInput=$QueuedInput.IsPresent
        fixture='pre-login local notification; not STATE_STARTED acceptance'
        referenceRevision=$ReferenceRevision
        viewer=$start.FileName; viewerSha256=$viewerHash
        driverSha256=(Get-FileHash $Driver).Hash; captureSha256=(Get-FileHash $CaptureHelper).Hash
        backgroundUrl=$pageUrl; backgroundSha256=$pageHash; backgroundPath=$pagePath
        notification=$Notification; substitutions=$Substitutions; locale=$Language
        requestedWidth=$Width; requestedHeight=$Height; requestedUiScale=$UiScale
        inactiveFocus=$InactiveFocus.IsPresent
        requestSha256=(Get-FileHash (Join-Path $root 'capture-request.xml')).Hash
        requestedMaximized=$Maximized.IsPresent; actualMaximized=$actualMaximized
        requestedAnisotropy=$Anisotropy
        loginButtonStates=$LoginButtonStates.IsPresent
        sequence=$Sequence
        tearOff=$TearOff.IsPresent
        tearOffLifecycle=$TearOffLifecycle.IsPresent
        helpBrowser=$HelpBrowser.IsPresent
        dialogLifecycle=$DialogLifecycle.IsPresent
        dialogMovement=$DialogMovement.IsPresent
        controlledReplay=$ControlledReplay.IsPresent
        buttonStates=@($ButtonStates)
        focusInput='injected WM_KILLFOCUS/WM_SETFOCUS; not external-window activation acceptance'
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