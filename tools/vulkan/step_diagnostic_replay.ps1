param(
    [Parameter(Mandatory=$true)][int]$ViewerProcessId,
    [Parameter(Mandatory=$true)][string]$Directory,
    [Parameter(Mandatory=$true)][ValidateRange(1,2147483647)][int]$Sequence,
    [Parameter(Mandatory=$true)][ValidateRange(-1,9223372036854775807)][long]$AgeMicroseconds,
    [ValidateRange(1,50)][int]$TimeoutSeconds=30
)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $Directory).Path
$process=Get-Process -Id $ViewerProcessId -ErrorAction Stop
function Read-ReplayText([string]$Path) {
    $stream=[IO.FileStream]::new($Path,[IO.FileMode]::Open,[IO.FileAccess]::Read,([IO.FileShare]::ReadWrite -bor [IO.FileShare]::Delete))
    $reader=[IO.StreamReader]::new($stream)
    try { return $reader.ReadToEnd() } finally { $reader.Dispose() }
}
if (-not ('DiagnosticReplayWake' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class DiagnosticReplayWake {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    public static extern IntPtr OpenEvent(uint access, bool inherit, string name);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool SetEvent(IntPtr handle);
    [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
}
'@
}
$eventHandle=[DiagnosticReplayWake]::OpenEvent(2,$false,"Local\VulkanStormReplay-$ViewerProcessId")
if ($eventHandle -eq [IntPtr]::Zero) { throw 'Viewer has no diagnostic replay event; no request was submitted.' }
$watcher=[IO.FileSystemWatcher]::new($root)
try {
    $watcher.EnableRaisingEvents=$true
    $request=Join-Path $root 'request.txt'
    if (Test-Path -LiteralPath $request) {
        $previous=([IO.File]::ReadAllText($request).Trim() -split '\s+')
        if ($previous.Count -ne 2 -or $Sequence -le [long]$previous[0]) { throw 'Replay sequence must increase.' }
    }
    $temporary=Join-Path $root "request-$Sequence.tmp"
    [IO.File]::WriteAllText($temporary,"$Sequence $AgeMicroseconds`n",[Text.UTF8Encoding]::new($false))
    [IO.File]::Move($temporary,$request,$true)
    if (![DiagnosticReplayWake]::SetEvent($eventHandle)) { throw 'Cannot wake diagnostic viewer.' }
    $deadline=[DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $ack=Join-Path $root "ack-$Sequence.txt"
    $failure=Join-Path $root 'failure.txt'
    do {
        if (Test-Path -LiteralPath $failure) { throw ('Diagnostic replay failed: '+[IO.File]::ReadAllText($failure)) }
        if (Test-Path -LiteralPath $ack) {
            $fields=(Read-ReplayText $ack).Trim() -split '\s+'
            if ($fields.Count -eq 3 -and [long]$fields[0] -eq $Sequence) {
                $expected=if ($AgeMicroseconds -eq -1) { 'released' } else { 'frame' }
                if ($fields[2] -ne $expected -or ($AgeMicroseconds -ge 0 -and [long]$fields[1] -ne $AgeMicroseconds)) {
                    throw 'Diagnostic acknowledgement does not match the requested clock step.'
                }
                return [pscustomobject]@{Sequence=$Sequence;AgeMicroseconds=[long]$fields[1];State=$fields[2];Process=$ViewerProcessId}
            }
        }
        if ($process.HasExited) { throw 'Viewer exited before acknowledging the replay step.' }
        $remaining=[int]($deadline-[DateTime]::UtcNow).TotalMilliseconds
        if ($remaining -le 0) { throw 'Replay acknowledgement timeout; viewer was not terminated.' }
        [void]$watcher.WaitForChanged([IO.WatcherChangeTypes]::All,[Math]::Min($remaining,1000))
    } while ($true)
} finally {
    $watcher.Dispose()
    [void][DiagnosticReplayWake]::CloseHandle($eventHandle)
    $process.Dispose()
}