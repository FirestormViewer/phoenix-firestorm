param(
    [Parameter(Mandatory=$true)][string]$Reference,
    [Parameter(Mandatory=$true)][string]$Candidate,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
function Read-Capture([string]$Path) {
    $bytes=[IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Path))
    if ($bytes.Length -lt 8) { throw "Truncated capture: $Path" }
    $width=[BitConverter]::ToUInt32($bytes,0)
    $height=[BitConverter]::ToUInt32($bytes,4)
    if (!$width -or !$height -or $width -gt 16384 -or $height -gt 16384 -or
        $bytes.LongLength -ne 8L+4L*$width*$height) { throw "Invalid capture dimensions: $Path" }
    [pscustomobject]@{Width=[int]$width; Height=[int]$height; Bytes=$bytes}
}
function Save-Png($Image,[string]$Path) {
    $bitmap=[Drawing.Bitmap]::new($Image.Width,$Image.Height,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $rectangle=[Drawing.Rectangle]::new(0,0,$Image.Width,$Image.Height)
        $data=$bitmap.LockBits($rectangle,[Drawing.Imaging.ImageLockMode]::WriteOnly,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $row=[byte[]]::new($Image.Width*4)
            for ($vertical=0; $vertical -lt $Image.Height; ++$vertical) {
                for ($horizontal=0; $horizontal -lt $Image.Width; ++$horizontal) {
                    $source=8+4*($vertical*$Image.Width+$horizontal)
                    $target=4*$horizontal
                    $row[$target]=$Image.Bytes[$source+2]
                    $row[$target+1]=$Image.Bytes[$source+1]
                    $row[$target+2]=$Image.Bytes[$source]
                    $row[$target+3]=$Image.Bytes[$source+3]
                }
                [Runtime.InteropServices.Marshal]::Copy($row,0,[IntPtr]::Add($data.Scan0,$vertical*$data.Stride),$row.Length)
            }
        } finally { $bitmap.UnlockBits($data) }
        $bitmap.Save($Path,[Drawing.Imaging.ImageFormat]::Png)
    } finally { $bitmap.Dispose() }
}
$baseline=Read-Capture $Reference
$actual=Read-Capture $Candidate
if ($baseline.Width -ne $actual.Width -or $baseline.Height -ne $actual.Height) { throw 'Capture dimensions differ; no rescaling is permitted.' }
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Output directory already exists; evidence will not be overwritten.' }
[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$difference=[pscustomobject]@{Width=$baseline.Width; Height=$baseline.Height; Bytes=[byte[]]::new($baseline.Bytes.Length)}
$differentPixels=0L
$maximum=[int[]]::new(4)
for ($offset=8; $offset -lt $baseline.Bytes.Length; $offset+=4) {
    $different=$false
    for ($channel=0; $channel -lt 4; ++$channel) {
        $delta=[Math]::Abs([int]$baseline.Bytes[$offset+$channel]-[int]$actual.Bytes[$offset+$channel])
        $maximum[$channel]=[Math]::Max($maximum[$channel],$delta)
        if ($delta) { $different=$true }
    }
    if ($different) { ++$differentPixels; $difference.Bytes[$offset]=255 }
    $difference.Bytes[$offset+3]=255
}
Save-Png $baseline (Join-Path $OutputDirectory 'reference.png')
Save-Png $actual (Join-Path $OutputDirectory 'candidate.png')
Save-Png $difference (Join-Path $OutputDirectory 'difference.png')
$report=[ordered]@{
    reference=(Resolve-Path -LiteralPath $Reference).Path
    candidate=(Resolve-Path -LiteralPath $Candidate).Path
    referenceSha256=(Get-FileHash -LiteralPath $Reference -Algorithm SHA256).Hash
    candidateSha256=(Get-FileHash -LiteralPath $Candidate -Algorithm SHA256).Hash
    width=$baseline.Width; height=$baseline.Height
    format='RGBA8 top-origin'; tolerance=0
    differingPixels=$differentPixels; maximumChannelErrors=$maximum
    exactMatch=($differentPixels -eq 0)
}
$report | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory 'comparison.json') -Encoding utf8
$report | ConvertTo-Json
if ($differentPixels) { exit 2 }