#Requires -Version 7.0

[CmdletBinding()]
param(
    [ValidateSet('OpenGL', 'Vulkan')]
    [string]$Backend = 'Vulkan',
    [ValidateSet('Startup', 'GLFontInitializationFailure')]
    [string]$ExpectedOutcome = 'Startup',
    [switch]$Validation,
    [switch]$CaptureGl,
    [ValidateRange(10, 120)]
    [int]$RunSeconds = 45,
    [ValidateRange(20, 300)]
    [int]$DeadlineSeconds = 90,
    [string]$Executable = (Join-Path $PSScriptRoot '../build-vc170-64/newview/RelWithDebInfo/vulkanstorm-bin.exe')
)

$ErrorActionPreference = 'Stop'
if (!$IsWindows) { throw 'This probe currently covers the Windows viewer lifecycle only.' }
if ($CaptureGl -and $Backend -ne 'OpenGL') { throw 'Native swapchain capture is not qualified by this probe.' }
if ($Validation -and $Backend -ne 'Vulkan') { throw 'Vulkan validation requires the Vulkan backend.' }
if ($ExpectedOutcome -eq 'GLFontInitializationFailure' -and $Backend -ne 'Vulkan') {
    throw 'The GL-font initialization regression applies to the native route.'
}
if ($DeadlineSeconds -le $RunSeconds) { throw 'DeadlineSeconds must exceed RunSeconds.' }

if (!('NativeUiStartupProbe.Waiter' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Diagnostics;
using System.IO;
using System.Threading;

namespace NativeUiStartupProbe
{
    public static class Waiter
    {
        public static string Wait(Process process, string root, string logPath, int timeoutMilliseconds)
        {
            using (var changed = new AutoResetEvent(false))
            using (var watcher = new FileSystemWatcher(root, "*.log"))
            {
                watcher.IncludeSubdirectories = true;
                watcher.NotifyFilter = NotifyFilters.LastWrite | NotifyFilters.Size | NotifyFilters.FileName;
                FileSystemEventHandler fileChanged = (sender, args) => changed.Set();
                ErrorEventHandler watcherError = (sender, args) => changed.Set();
                EventHandler processExited = (sender, args) => changed.Set();
                watcher.Changed += fileChanged;
                watcher.Created += fileChanged;
                watcher.Error += watcherError;
                process.EnableRaisingEvents = true;
                process.Exited += processExited;
                watcher.EnableRaisingEvents = true;
                var elapsed = Stopwatch.StartNew();
                try
                {
                    while (true)
                    {
                        if (process.HasExited) return "Exited";
                        try
                        {
                            if (File.Exists(logPath))
                            {
                                using (var stream = new FileStream(logPath, FileMode.Open, FileAccess.Read,
                                                                  FileShare.ReadWrite | FileShare.Delete))
                                using (var reader = new StreamReader(stream))
                                {
                                    if (reader.ReadToEnd().Contains(" ERROR #")) return "FatalLog";
                                }
                            }
                        }
                        catch (IOException) {}
                        int remaining = timeoutMilliseconds - (int)elapsed.ElapsedMilliseconds;
                        if (remaining <= 0 || !changed.WaitOne(remaining)) return "Deadline";
                    }
                }
                finally
                {
                    watcher.EnableRaisingEvents = false;
                    watcher.Changed -= fileChanged;
                    watcher.Created -= fileChanged;
                    watcher.Error -= watcherError;
                    process.Exited -= processExited;
                }
            }
        }
    }
}
'@
}

$executableFile = Get-Item -LiteralPath $Executable
foreach ($resource in @('app_settings', 'skins', 'fonts', 'ca-bundle.crt')) {
    if (!(Test-Path -LiteralPath (Join-Path $executableFile.DirectoryName $resource))) {
        throw "Missing staged resource: $resource"
    }
}

$repository = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$runName = 'native-ui-probe-' + $Backend.ToLowerInvariant() + '-' + [guid]::NewGuid().ToString('N')
$root = Join-Path $repository ('logs/' + $runName)
if (Test-Path -LiteralPath $root) { throw 'Refusing to reuse a profile directory.' }
foreach ($directory in @('roaming', 'local', 'temp')) {
    [void][System.IO.Directory]::CreateDirectory((Join-Path $root $directory))
}
$logPath = Join-Path $root 'roaming/Vulkanstorm_x64/logs/Vulkanstorm.log'
$capturePath = Join-Path $root 'gl-startup.rgba'
$start = [System.Diagnostics.ProcessStartInfo]::new($executableFile.FullName)
$start.UseShellExecute = $false
$start.WorkingDirectory = $executableFile.DirectoryName
$start.RedirectStandardError = $true
$start.RedirectStandardOutput = $true
$start.Environment['APPDATA'] = Join-Path $root 'roaming'
$start.Environment['LOCALAPPDATA'] = Join-Path $root 'local'
$start.Environment['TEMP'] = Join-Path $root 'temp'
$start.Environment['TMP'] = Join-Path $root 'temp'
$start.Environment['PRELOG'] = Join-Path $root 'directory-prelog.txt'
[void]$start.Environment.Remove('VULKANSTORM_CAPTURE')
if ($CaptureGl) { $start.Environment['VULKANSTORM_CAPTURE'] = $capturePath }
if ($Validation) {
    foreach ($variable in @('VK_INSTANCE_LAYERS', 'VK_LOADER_LAYERS_ENABLE', 'VK_LOADER_LAYERS_ALLOW')) {
        [void]$start.Environment.Remove($variable)
    }
    $start.Environment['VK_LOADER_LAYERS_DISABLE'] = 'VK_LAYER_LUNARG_api_dump'
    $start.Environment['VK_LOADER_DEBUG'] = 'error,warn,layer'
}

$arguments = @(
    '--set', 'RenderBackend', $Backend,
    '--set', 'RenderVulkanDebug', $Validation.IsPresent.ToString().ToLowerInvariant(),
    '--set', 'AutoLogin', 'false',
    '--set', 'NvAPICreateApplicationProfile', 'false',
    '--set', 'DisableCrashLogger', 'true',
    '--set', 'AllowMultipleViewers', 'true',
    '--set', 'FullScreen', 'false',
    '--set', 'WindowMaximized', 'false',
    '--set', 'FirstLoginThisInstall', 'false',
    '--set', 'WindowWidth', '1024',
    '--set', 'WindowHeight', '768',
    '--set', 'ShowConsoleWindow', 'false',
    '--quitafter', $RunSeconds.ToString([cultureinfo]::InvariantCulture)
)
foreach ($argument in $arguments) { $start.ArgumentList.Add($argument) }

$process = $null
$stderrFile = [System.IO.File]::Create((Join-Path $root 'loader-stderr.txt'))
$stdoutFile = [System.IO.File]::Create((Join-Path $root 'process-stdout.txt'))
$elapsed = [System.Diagnostics.Stopwatch]::StartNew()
$reason = 'NotStarted'
$terminated = $false
try {
    $process = [System.Diagnostics.Process]::Start($start)
    $stderrCopy = $process.StandardError.BaseStream.CopyToAsync($stderrFile)
    $stdoutCopy = $process.StandardOutput.BaseStream.CopyToAsync($stdoutFile)
    Write-Host "Started $Backend probe PID=$($process.Id); artifacts: $root"
    $reason = [NativeUiStartupProbe.Waiter]::Wait($process, $root, $logPath, $DeadlineSeconds * 1000)
    if (!$process.HasExited) {
        $terminated = $true
        $process.Kill($true)
    }
    $process.WaitForExit()
    $exitCode = $process.ExitCode
    $processId = $process.Id
    $stderrCopy.GetAwaiter().GetResult()
    $stdoutCopy.GetAwaiter().GetResult()
}
finally {
    if ($process) {
        if (!$process.HasExited) { $process.Kill($true); $process.WaitForExit() }
        $process.Dispose()
    }
    $stderrFile.Dispose()
    $stdoutFile.Dispose()
    $elapsed.Stop()
}

$log = if (Test-Path -LiteralPath $logPath) { Get-Content -LiteralPath $logPath -Raw } else { '' }
$loaderOutput = Get-Content -LiteralPath (Join-Path $root 'loader-stderr.txt') -Raw
$effectiveBackend = $log.Contains("Render backend: $Backend")
$nativeSession = $log.Contains('Session: Vulkan owns the viewer window')
$loginReached = $log.Contains('STATE_LOGIN_WAIT')
$fontFailure = $log.Contains('LLImageGL::createGLTexture : ASSERT (gGLManager.mInited)')
$fatal = $log.Contains(' ERROR #')
$validationActivated = $nativeSession -and
    ($loaderOutput -match 'Insert instance layer "VK_LAYER_KHRONOS_validation"')
$startupPassed = !$terminated -and $exitCode -eq 0 -and !$fatal -and $effectiveBackend -and
    $loginReached -and ($Backend -ne 'Vulkan' -or $nativeSession)
$capture = $null
if ($CaptureGl -and (Test-Path -LiteralPath $capturePath)) {
    $bytes = [System.IO.File]::ReadAllBytes($capturePath)
    if ($bytes.Length -ge 8) {
        $width = [BitConverter]::ToUInt32($bytes, 0)
        $height = [BitConverter]::ToUInt32($bytes, 4)
        $capture = [ordered]@{
            Width = $width
            Height = $height
            ByteCount = $bytes.LongLength
            StructurallyValid = ($width -gt 0 -and $height -gt 0 -and $bytes.LongLength -eq (8L + 4L * $width * $height))
            SHA256 = (Get-FileHash -LiteralPath $capturePath -Algorithm SHA256).Hash
        }
    }
}
$expectationMatched = if ($ExpectedOutcome -eq 'GLFontInitializationFailure') {
    $effectiveBackend -and $fontFailure -and !$nativeSession -and !$loginReached -and $reason -ne 'Deadline'
} else {
    $startupPassed -and (!$Validation -or $validationActivated) -and
        (!$CaptureGl -or ($capture -and $capture.StructurallyValid))
}
$result = [ordered]@{
    Backend = $Backend
    ExpectedOutcome = $ExpectedOutcome
    ExpectationMatched = [bool]$expectationMatched
    StartupPassed = [bool]$startupPassed
    FontInitializationFailure = $fontFailure
    NativeSessionReached = $nativeSession
    LoginReached = $loginReached
    ValidationRequested = $Validation.IsPresent
    ValidationActivationObserved = [bool]$validationActivated
    StopReason = $reason
    TerminatedByProbe = $terminated
    ExitCode = $exitCode
    ProcessId = $processId
    ElapsedSeconds = $elapsed.Elapsed.TotalSeconds
    Executable = $executableFile.FullName
    ExecutableSHA256 = (Get-FileHash -LiteralPath $executableFile.FullName -Algorithm SHA256).Hash
    Arguments = $arguments
    ArtifactDirectory = $root
    Capture = $capture
    Qualification = 'Startup/regression observation only; not full UI, GPU safety, or parity qualification.'
}
$json = $result | ConvertTo-Json -Depth 5
[System.IO.File]::WriteAllText((Join-Path $root 'result.json'), $json)
$json
if (!$expectationMatched) { throw "Startup probe did not meet '$ExpectedOutcome'; inspect $root" }