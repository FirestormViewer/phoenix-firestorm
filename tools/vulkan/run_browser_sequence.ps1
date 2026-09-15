param(
    [Parameter(Mandatory=$true)][int]$ViewerProcessId,
    [Parameter(Mandatory=$true)][string]$CaptureHelper,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [Parameter(Mandatory=$true)][ValidateSet('gl','native')][string]$Backend,
    [switch]$HoverOnly,
    [switch]$Dialogs,
    [switch]$QueuedInput,
    [switch]$Continuous
)
$ErrorActionPreference='Stop'
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
    if ($QueuedInput) {
        if (![BrowserSequencePostedInput]::PostMessage($window,$Message,[IntPtr]$Parameter,[IntPtr]$Data)) { throw 'Browser sequence input could not be queued.' }
        return
    }
    [BrowserSequenceInput]::SendMessage($window,$Message,[IntPtr]$Parameter,[IntPtr]$Data) | Out-Null
}
function Move-SequencePointer([int]$Horizontal,[int]$Vertical) {
    $point=[BrowserSequenceInput+Point]::new(); $point.x=$Horizontal; $point.y=$Vertical
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
    if ($Dialogs) {
        Capture-SequenceState 'menu-open' { Click-SequencePointer 32 9 }
        Capture-SequenceState 'preferences-click' { Click-SequencePointer 80 34 }
        Capture-SequenceState 'preferences' { Move-SequencePointer 2200 900 }
        Capture-SequenceState 'colors-click' { Click-SequencePointer 990 519 }
        Capture-SequenceState 'colors' { Move-SequencePointer 2200 900 }
        Capture-SequenceState 'color-picker-click' { Click-SequencePointer 1135 544 }
        Capture-SequenceState 'color-picker' { Move-SequencePointer 2200 900 }
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
    [ordered]@{sequence=$(if ($Dialogs) { 'dialogs' } else { 'browser' });queuedInput=$QueuedInput.IsPresent;completed=$true;states=$(if ($Dialogs) { 7 } elseif ($HoverOnly) { 2 } else { 10 });process=$ViewerProcessId} | ConvertTo-Json |
        Set-Content (Join-Path $OutputDirectory 'sequence-complete.json') -Encoding utf8
} finally {
    Send-SequenceMessage 0x202 0 ((400 -shl 16) -bor 1000)
    [BrowserSequenceInput]::SetCursorPos($saved.x,$saved.y) | Out-Null
}