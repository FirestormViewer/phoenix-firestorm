param(
    [Parameter(Mandatory=$true)][int]$ViewerProcessId,
    [Parameter(Mandatory=$true)][string]$CaptureHelper,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [Parameter(Mandatory=$true)][ValidateSet('gl','native')][string]$Backend,
    [ValidateRange(0.75,2.0)][double]$UiScale=1.0,
    [switch]$HoverOnly,
    [switch]$Dialogs,
    [switch]$TearOff,
    [switch]$TearOffLifecycle,
    [switch]$HelpBrowser,
    [switch]$DialogLifecycle,
    [switch]$DialogMovement,
    [switch]$ControlledReplay,
    [switch]$QueuedInput,
    [switch]$Continuous
)
$ErrorActionPreference='Stop'
if ($ControlledReplay -and (!$Dialogs -or !$TearOff -or !$QueuedInput)) { throw 'Controlled replay requires queued tear-off input.' }
$replaySequence=0
$replayDirectory=Join-Path $OutputDirectory 'replay'
function Convert-SequencePoint([int]$Horizontal,[int]$Vertical) {
    if ($uiScale -eq 1 -or ($Horizontal -eq 2200 -and $Vertical -eq 900)) { return @($Horizontal,$Vertical) }
    if ($Horizontal -lt 500 -and $Vertical -lt 400) {
        return @([int][Math]::Floor($Horizontal*$uiScale),[int][Math]::Floor($Vertical*$uiScale))
    }
    return @([int][Math]::Floor(1280+($Horizontal-1280)*$uiScale),[int][Math]::Floor(684.5+($Vertical-684.5)*$uiScale))
}
if (-not ('BrowserSequencePostedInput' -as [type])) { Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class BrowserSequencePostedInput {
    [DllImport("user32.dll", SetLastError=true)] public static extern bool PostMessage(IntPtr window,uint message,IntPtr parameter,IntPtr data);
}
'@ }
if (-not ('BrowserSequenceKeys' -as [type])) { Add-Type -TypeDefinition @'
using System.Runtime.InteropServices;
public static class BrowserSequenceKeys {
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint key,uint mode);
}
'@ }
if (-not ('BrowserSequenceInput' -as [type])) { Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class BrowserSequenceInput {
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int x,y; }
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int left,top,right,bottom; }
    public delegate bool EnumCallback(IntPtr window,IntPtr data);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumCallback callback,IntPtr data);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window,out uint process);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr window,out Rect rect);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr window,uint message,IntPtr parameter,IntPtr data);
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out Point point);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr window,ref Point point);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x,int y);
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint key,uint mode);
    public static IntPtr Find(int process) {
        IntPtr result=IntPtr.Zero;
        EnumWindows((window,data)=> {
            uint owner; GetWindowThreadProcessId(window,out owner);
            Rect rect;
            if(owner==process && IsWindowVisible(window) && GetClientRect(window,out rect) && rect.right==2560 && rect.bottom==1369) {
                result=window; return false;
            }
            return true;
        },IntPtr.Zero);
        return result;
    }
}
'@ }
$window=[BrowserSequenceInput]::Find($ViewerProcessId)
if ($window -eq [IntPtr]::Zero) { throw 'Browser sequence requires the qualified 2560x1369 client.' }
$saved=[BrowserSequenceInput+Point]::new()
if (![BrowserSequenceInput]::GetCursorPos([ref]$saved)) { throw 'Cannot preserve cursor.' }
$timeline=[Collections.Generic.List[object]]::new()
$clock=[Diagnostics.Stopwatch]::StartNew()
function Send-SequenceMessage([uint32]$Message,[long]$Parameter,[long]$Data) {
    if ($Message -in @(0x200,0x201,0x202)) {
        $point=Convert-SequencePoint ($Data -band 0xffff) (($Data -shr 16) -band 0xffff)
        $Data=($point[1] -shl 16) -bor ($point[0] -band 0xffff)
    }
    if ($QueuedInput) {
        if (![BrowserSequencePostedInput]::PostMessage($window,$Message,[IntPtr]$Parameter,[IntPtr]$Data)) { throw 'Browser sequence input could not be queued.' }
        return
    }
    [BrowserSequenceInput]::SendMessage($window,$Message,[IntPtr]$Parameter,[IntPtr]$Data) | Out-Null
}
function Move-SequencePointer([int]$Horizontal,[int]$Vertical) {
    $scaled=Convert-SequencePoint $Horizontal $Vertical
    $point=[BrowserSequenceInput+Point]::new(); $point.x=$scaled[0]; $point.y=$scaled[1]
    if (![BrowserSequenceInput]::ClientToScreen($window,[ref]$point) -or
        ![BrowserSequenceInput]::SetCursorPos($point.x,$point.y)) { throw 'Cannot position sequence cursor.' }
    Send-SequenceMessage 0x200 0 (($Vertical -shl 16) -bor $Horizontal)
}
function Click-SequencePointer([int]$Horizontal,[int]$Vertical) {
    Move-SequencePointer $Horizontal $Vertical
    $point=($Vertical -shl 16) -bor $Horizontal
    Send-SequenceMessage 0x201 1 $point
    Send-SequenceMessage 0x202 0 $point
}
function Send-SequenceKey([int]$Key) {
    $scan=[BrowserSequenceKeys]::MapVirtualKey($Key,0)
    $flags=1L -bor ([long]$scan -shl 16)
    if ($Key -in @(0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x2D,0x2E)) { $flags=$flags -bor 0x1000000L }
    Send-SequenceMessage 0x100 $Key $flags
    Send-SequenceMessage 0x101 $Key ($flags -bor 0xC0000000L)
}
function Capture-SequenceState([string]$State,[scriptblock]$Action) {
    if ($Continuous) {
        $streamStart=[Diagnostics.ProcessStartInfo]::new((Resolve-Path $CaptureHelper).Path)
        $streamStart.UseShellExecute=$false; $streamStart.RedirectStandardOutput=$true
        foreach ($argument in @('--stream',$window.ToInt64().ToString(),(Join-Path $OutputDirectory "$Backend-$State-stream"),'2000')) {
            $streamStart.ArgumentList.Add($argument)
        }
        $stream=[Diagnostics.Process]::Start($streamStart)
        try {
            $ready=$stream.StandardOutput.ReadLineAsync()
            if (!$ready.Wait(15000) -or $ready.Result -ne 'READY') { throw 'Continuous WGC did not arm before input.' }
            $actionStart=[Diagnostics.Stopwatch]::GetTimestamp()
            & $Action
            $actionEnd=[Diagnostics.Stopwatch]::GetTimestamp()
            if (!$stream.WaitForExit(15000) -or $stream.ExitCode -ne 0) { throw 'Continuous WGC did not complete successfully.' }
            $timeline.Add([ordered]@{state=$State;kind='continuous';actionStartQpc=$actionStart;actionEndQpc=$actionEnd;qpcFrequency=[Diagnostics.Stopwatch]::Frequency;directory="$Backend-$State-stream"})
        } finally { $stream.Dispose() }
        foreach ($sample in 0,1) {
            $path=Join-Path $OutputDirectory "$Backend-browser-$State-settled-$sample.rgba"
            & $CaptureHelper $window.ToInt64().ToString() $path
            if ($LASTEXITCODE -ne 0) { throw 'Continuous sequence settled capture failed.' }
        }
        return
    }
    $started=$clock.Elapsed.TotalMilliseconds
    & $Action
    $dispatched=$clock.Elapsed.TotalMilliseconds
    $frame=0
    do {
        $path=Join-Path $OutputDirectory "$Backend-browser-$State-transition-$frame.rgba"
        $before=$clock.Elapsed.TotalMilliseconds
        & $CaptureHelper $window.ToInt64().ToString() $path
        if ($LASTEXITCODE -ne 0) { throw 'Browser transition capture failed.' }
        $timeline.Add([ordered]@{state=$State;kind='transition';sample=$frame;actionStartMs=$started;actionEndMs=$dispatched;captureStartMs=$before;captureEndMs=$clock.Elapsed.TotalMilliseconds;file=[IO.Path]::GetFileName($path)})
        ++$frame
    } while ($clock.Elapsed.TotalMilliseconds-$dispatched -lt 2000 -and $frame -lt 60)
    foreach ($sample in 0,1) {
        $path=Join-Path $OutputDirectory "$Backend-browser-$State-settled-$sample.rgba"
        $before=$clock.Elapsed.TotalMilliseconds
        & $CaptureHelper $window.ToInt64().ToString() $path
        if ($LASTEXITCODE -ne 0) { throw 'Browser settled capture failed.' }
        $timeline.Add([ordered]@{state=$State;kind='settled';sample=$sample;actionStartMs=$started;actionEndMs=$dispatched;captureStartMs=$before;captureEndMs=$clock.Elapsed.TotalMilliseconds;file=[IO.Path]::GetFileName($path)})
    }
    $cursor=[BrowserSequenceInput+Point]::new()
    if (![BrowserSequenceInput]::GetCursorPos([ref]$cursor)) { throw 'Cannot observe sequence cursor.' }
    $timeline.Add([ordered]@{state=$State;kind='cursor';x=$cursor.x;y=$cursor.y;timeMs=$clock.Elapsed.TotalMilliseconds})
}
try {
    if ($ControlledReplay) {
        Capture-SequenceState 'controlled-ready' { Click-SequencePointer 1000 400 }
        Capture-SequenceState 'controlled-menu-open' { Click-SequencePointer 32 9 }
        foreach ($age in @(0,50000,100000,150000,200000,250000,300000)) {
            if ($age -eq 50000) { Click-SequencePointer 30 24 }
            ++$replaySequence
            $ack=& (Join-Path $PSScriptRoot 'step_diagnostic_replay.ps1') -ViewerProcessId $ViewerProcessId -Directory $replayDirectory -Sequence $replaySequence -AgeMicroseconds $age
            $timeline.Add([ordered]@{state="controlled-tearoff-$age";kind='controlled';sequence=$replaySequence;ageMicroseconds=$age;acknowledged=$ack.State})
            foreach ($sample in 0,1) {
                & $CaptureHelper $window.ToInt64().ToString() (Join-Path $OutputDirectory "$Backend-browser-controlled-tearoff-$age-settled-$sample.rgba")
                if ($LASTEXITCODE -ne 0) { throw 'Controlled frame capture failed.' }
            }
        }
        $effects=@(
            @{age=350000;state='effects-close-menu';action={ Click-SequencePointer 182 32; Click-SequencePointer 1000 400 }},
            @{age=400000;state='effects-menu';action={ Click-SequencePointer 32 9 }},
            @{age=450000;state='effects-preferences';action={ Click-SequencePointer 80 34 }},
            @{age=500000;state='effects-title';action={ Click-SequencePointer 1000 422 }},
            @{age=550000;state='glow-rise-1';action={ Move-SequencePointer 1572 422 }},
            @{age=600000;state='glow-rise-2';action={}},
            @{age=650000;state='glow-rise-3';action={}},
            @{age=700000;state='glow-decay-1';action={ Move-SequencePointer 2200 900 }},
            @{age=750000;state='glow-decay-2';action={}},
            @{age=800000;state='glow-decay-3';action={}},
            @{age=850000;state='editor-focus';action={ Click-SequencePointer 1100 446 }},
            @{age=900000;state='editor-focus-flash';action={}},
            @{age=1800000;state='editor-delay-boundary';action={}},
            @{age=2050000;state='editor-blink-off';action={}},
            @{age=2300000;state='editor-blink-on';action={}},
            @{age=2550000;state='editor-blink-off-again';action={}},
            @{age=2600000;state='editor-input-reset';action={ Send-SequenceMessage 0x102 97 1 }},
            @{age=2650000;state='editor-delete-reset';action={ Send-SequenceKey 0x08 }},
            @{age=2700000;state='button-pressed';action={ Move-SequencePointer 1560 956; Send-SequenceMessage 0x201 1 ((956 -shl 16) -bor 1560) }},
            @{age=2750000;state='button-release-outside';action={ Move-SequencePointer 2200 900; Send-SequenceMessage 0x202 0 ((900 -shl 16) -bor 2200) }}
        )
        foreach ($effect in $effects) {
            & $effect.action
            ++$replaySequence
            $ack=& (Join-Path $PSScriptRoot 'step_diagnostic_replay.ps1') -ViewerProcessId $ViewerProcessId -Directory $replayDirectory -Sequence $replaySequence -AgeMicroseconds $effect.age
            $timeline.Add([ordered]@{state=$effect.state;kind='controlled';sequence=$replaySequence;ageMicroseconds=$effect.age;acknowledged=$ack.State})
            foreach ($sample in 0,1) {
                & $CaptureHelper $window.ToInt64().ToString() (Join-Path $OutputDirectory "$Backend-browser-$($effect.state)-settled-$sample.rgba")
                if ($LASTEXITCODE -ne 0) { throw 'Controlled effects capture failed.' }
            }
        }
        ++$replaySequence
        & (Join-Path $PSScriptRoot 'step_diagnostic_replay.ps1') -ViewerProcessId $ViewerProcessId -Directory $replayDirectory -Sequence $replaySequence -AgeMicroseconds -1 | Out-Null
        $replaySequence=0
    }
    elseif ($Dialogs) {
        Capture-SequenceState 'menu-open' { Click-SequencePointer 32 9 }
        if ($TearOff) {
            Capture-SequenceState 'menu-detached' { Click-SequencePointer 30 24 }
            Capture-SequenceState 'menu-detached-away' { Move-SequencePointer 2200 900 }
            if ($TearOffLifecycle) {
                Capture-SequenceState 'menu-drag-pressed' {
                    Move-SequencePointer 40 32
                    Send-SequenceMessage 0x201 1 ((32 -shl 16) -bor 40)
                }
                Capture-SequenceState 'menu-drag-moved' {
                    Move-SequencePointer 240 232
                }
                Capture-SequenceState 'menu-detached-moved' {
                    Send-SequenceMessage 0x202 0 ((232 -shl 16) -bor 240)
                }
                Capture-SequenceState 'menu-detached-closed' { Click-SequencePointer 382 232 }
                Capture-SequenceState 'menu-reattached' { Click-SequencePointer 32 9 }
                Capture-SequenceState 'menu-detached-reopened' { Click-SequencePointer 30 24 }
            }
        } else {
        Capture-SequenceState 'preferences-click' { Click-SequencePointer 80 34 }
        Capture-SequenceState 'preferences' { Move-SequencePointer 2200 900 }
        if ($HelpBrowser) {
            Capture-SequenceState 'help-browser' { Click-SequencePointer 1572 422 }
            Capture-SequenceState 'help-browser-away' { Move-SequencePointer 2200 900 }
            if ($DialogMovement) {
                Capture-SequenceState 'help-field-focus' { Click-SequencePointer 1320 550 }
                Capture-SequenceState 'help-field-edited' {
                    Send-SequenceKey 0x24
                    foreach ($index in 1..10) { Send-SequenceKey 0x2E }
                    foreach ($character in 'Help input'.ToCharArray()) { Send-SequenceMessage 0x102 ([int]$character) 1 }
                    Click-SequencePointer 1578 550
                    Move-SequencePointer 2200 900
                }
                Capture-SequenceState 'help-title-pressed' { Move-SequencePointer 1100 403; Send-SequenceMessage 0x201 1 ((403 -shl 16) -bor 1100) }
                Capture-SequenceState 'help-title-moved' { Move-SequencePointer 1000 303 }
                Capture-SequenceState 'help-title-released' { Send-SequenceMessage 0x202 0 ((303 -shl 16) -bor 1000) }
                Capture-SequenceState 'help-size-pressed' { Move-SequencePointer 1502 887; Send-SequenceMessage 0x201 1 ((887 -shl 16) -bor 1502) }
                Capture-SequenceState 'help-size-moved' { Move-SequencePointer 1582 947 }
                Capture-SequenceState 'help-size-released' { Send-SequenceMessage 0x202 0 ((947 -shl 16) -bor 1582); Move-SequencePointer 2200 900 }
                if ($env:LL_DIAGNOSTIC_HELP_RESIZE_DWELL -eq '1') {
                    foreach ($segment in 0..2) {
                        $directory=Join-Path $OutputDirectory "$Backend-help-resize-dwell-$segment"
                        $started=[Diagnostics.Stopwatch]::GetTimestamp()
                        & $CaptureHelper --stream $window.ToInt64().ToString() $directory 10000
                        if ($LASTEXITCODE -ne 0) { throw 'Help resize dwell capture failed.' }
                        $timeline.Add([ordered]@{state="help-resize-dwell-$segment";kind='no-input';startQpc=$started;endQpc=[Diagnostics.Stopwatch]::GetTimestamp();qpcFrequency=[Diagnostics.Stopwatch]::Frequency;directory=$directory})
                    }
                }
                Capture-SequenceState 'help-size-restore-pressed' { Move-SequencePointer 1582 947; Send-SequenceMessage 0x201 1 ((947 -shl 16) -bor 1582) }
                Capture-SequenceState 'help-size-restored' { Move-SequencePointer 1502 887; Send-SequenceMessage 0x202 0 ((887 -shl 16) -bor 1502) }
                Capture-SequenceState 'help-position-restore-pressed' { Move-SequencePointer 1000 303; Send-SequenceMessage 0x201 1 ((303 -shl 16) -bor 1000) }
                Capture-SequenceState 'help-position-restored' { Move-SequencePointer 1100 403; Send-SequenceMessage 0x202 0 ((403 -shl 16) -bor 1100); Move-SequencePointer 2200 900 }
            }
            if ($DialogLifecycle) {
                Capture-SequenceState 'help-closed' { Click-SequencePointer 1595 403 }
                Capture-SequenceState 'help-reopened' { Click-SequencePointer 1572 422 }
                Capture-SequenceState 'help-reopened-away' { Move-SequencePointer 2200 900 }
            }
        } else {
        Capture-SequenceState 'colors-click' { Click-SequencePointer 990 519 }
        Capture-SequenceState 'colors' { Move-SequencePointer 2200 900 }
        Capture-SequenceState 'color-picker-click' { Click-SequencePointer 1135 544 }
        Capture-SequenceState 'color-picker' { Move-SequencePointer 2200 900 }
        if ($DialogLifecycle) {
            Capture-SequenceState 'picker-closed' { Click-SequencePointer 2046 422 }
            Capture-SequenceState 'picker-reopened' { Click-SequencePointer 1135 544 }
            Capture-SequenceState 'picker-reopened-away' { Move-SequencePointer 2200 900 }
            if ($DialogMovement) {
                Capture-SequenceState 'picker-parent-pressed' {
                    Move-SequencePointer 1000 422
                    Send-SequenceMessage 0x201 1 ((422 -shl 16) -bor 1000)
                }
                Capture-SequenceState 'picker-parent-dragged' { Move-SequencePointer 800 322 }
                Capture-SequenceState 'picker-parent-released' { Send-SequenceMessage 0x202 0 ((322 -shl 16) -bor 800) }
                Capture-SequenceState 'picker-parent-closed' { Click-SequencePointer 1406 322 }
            } else {
                Capture-SequenceState 'picker-parent-closed' { Click-SequencePointer 1606 422 }
            }
        }
        }
        }
    }
    else {
    Capture-SequenceState 'ready' { Click-SequencePointer 1000 400 }
    Capture-SequenceState 'hover' { Move-SequencePointer 618 120 }
    if (!$HoverOnly) {
    Capture-SequenceState 'pressed' { Send-SequenceMessage 0x201 1 ((120 -shl 16) -bor 618) }
    Capture-SequenceState 'released' { Send-SequenceMessage 0x202 0 ((120 -shl 16) -bor 618); Click-SequencePointer 1000 400 }
    Capture-SequenceState 'field-focus' { Click-SequencePointer 400 120 }
    Capture-SequenceState 'edited' {
        Send-SequenceKey 0x24
        foreach ($index in 1..10) { Send-SequenceKey 0x2E }
        foreach ($character in 'Fixture text'.ToCharArray()) { Send-SequenceMessage 0x102 ([int]$character) 1 }
        Click-SequencePointer 618 120
        Click-SequencePointer 1000 400
    }
    Capture-SequenceState 'select-open' { Click-SequencePointer 547 120 }
    Capture-SequenceState 'select-changed' { Send-SequenceKey 0x28; Send-SequenceKey 0x0D; Click-SequencePointer 1000 400 }
    Capture-SequenceState 'scrolled' {
        Move-SequencePointer 1000 400
        $point=[BrowserSequenceInput+Point]::new(); $point.x=1000; $point.y=400
        [BrowserSequenceInput]::ClientToScreen($window,[ref]$point) | Out-Null
        Send-SequenceMessage 0x20A (-960 -shl 16) (($point.y -shl 16) -bor $point.x)
    }
    Capture-SequenceState 'scroll-reset' {
        $point=[BrowserSequenceInput+Point]::new(); $point.x=1000; $point.y=400
        [BrowserSequenceInput]::ClientToScreen($window,[ref]$point) | Out-Null
        Send-SequenceMessage 0x20A (960 -shl 16) (($point.y -shl 16) -bor $point.x)
    }
    }
    }
    $timeline | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $OutputDirectory "$Backend-browser-timeline.json") -Encoding utf8
    [ordered]@{sequence=$(if ($Dialogs) { 'dialogs' } else { 'browser' });uiScale=$uiScale;controlledReplay=$ControlledReplay.IsPresent;dialogMovement=$DialogMovement.IsPresent;dialogLifecycle=$DialogLifecycle.IsPresent;helpBrowser=$HelpBrowser.IsPresent;tearOff=$TearOff.IsPresent;tearOffLifecycle=$TearOffLifecycle.IsPresent;lifecycleRevision=2;queuedInput=$QueuedInput.IsPresent;completed=$true;states=@($timeline | ForEach-Object { $_['state'] } | Select-Object -Unique).Count;process=$ViewerProcessId} | ConvertTo-Json |
        Set-Content (Join-Path $OutputDirectory 'sequence-complete.json') -Encoding utf8
} finally {
    if ($ControlledReplay -and $replaySequence) {
        & (Join-Path $PSScriptRoot 'step_diagnostic_replay.ps1') -ViewerProcessId $ViewerProcessId -Directory $replayDirectory -Sequence ($replaySequence+1) -AgeMicroseconds -1 | Out-Null
    }
    Send-SequenceMessage 0x202 0 ((400 -shl 16) -bor 1000)
    [BrowserSequenceInput]::SetCursorPos($saved.x,$saved.y) | Out-Null
}