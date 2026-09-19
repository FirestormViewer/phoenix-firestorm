param(
    [Parameter(Mandatory = $true)][string]$PackageInclude,
    [switch]$Bridge,
    [string]$VisualStudioRoot = 'C:/Program Files/Microsoft Visual Studio/2022/Community'
)
$ErrorActionPreference = 'Stop'
$PackageInclude = [IO.Path]::GetFullPath($PackageInclude)
$plugin = Split-Path $PSScriptRoot -Parent
$output = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../../.audio-test-build'))
$vcvars = Join-Path $VisualStudioRoot 'VC/Auxiliary/Build/vcvars64.bat'
if (!(Test-Path (Join-Path $PackageInclude 'soloud/miniaudio.h'))) { throw 'Published miniaudio header is required.' }
if (!(Test-Path $vcvars)) { throw 'VS2022 x64 compiler setup not found.' }
New-Item -ItemType Directory -Force $output | Out-Null
& cmake "-DVLC_MINIAUDIO_INPUT=$PackageInclude/soloud/miniaudio.h" "-DVLC_MINIAUDIO_OUTPUT=$output/llvlc_miniaudio.h" -P "$plugin/prepare_miniaudio.cmake"
if ($LASTEXITCODE -ne 0) { throw 'Owned miniaudio extension could not be prepared.' }
Set-Content -LiteralPath (Join-Path $output '.gitignore') -Value '*'
Push-Location $output
try {
    $command = 'call "{0}" >nul && cl /nologo /std:c++17 /permissive- /W4 /WX /EHsc /O2 /Zi /MD /DNDEBUG /I . /I "{2}" /I "{1}" "{2}/llvlcaudio.cpp" "{3}/llvlcaudio_test.cpp" /Fe:llvlcaudio_test.exe /link /DEBUG /INCREMENTAL:NO && llvlcaudio_test.exe' -f $vcvars, $PackageInclude, $plugin, $PSScriptRoot
    if ($Bridge) {
        $packages = Split-Path $PackageInclude -Parent
        $library = Get-ChildItem $packages -Filter libvlc.lib -Recurse | Select-Object -First 1
        $runtime = Get-ChildItem $packages -Filter libvlc.dll -Recurse | Select-Object -First 1
        if (!$library -or !$runtime) { throw 'Published VLC import library and runtime are required.' }
        $env:PATH = $runtime.DirectoryName + ';' + $env:PATH
        $command = 'call "{0}" >nul && cl /nologo /std:c++17 /permissive- /W4 /WX /wd4191 /EHsc /O2 /Zi /MD /DNDEBUG /I . /I "{1}" /I "{2}" "{2}/llvlcaudio.cpp" "{2}/llvlcaudiobridge.cpp" "{3}/llvlcaudiobridge_test.cpp" /Fe:llvlcaudiobridge_test.exe /link /DEBUG /INCREMENTAL:NO "{4}" && llvlcaudiobridge_test.exe' -f $vcvars, $PackageInclude, $plugin, $PSScriptRoot, $library.FullName
    }
    & cmd /d /c $command
    if ($LASTEXITCODE -ne 0) { throw "Audio tests failed: $LASTEXITCODE" }
}
finally { Pop-Location }