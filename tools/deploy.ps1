# Copy the built ROM to the flash cart SD card, if present.
# Rule: only when drive F: volume label is exactly "GBA" (case-sensitive).
$rom = Join-Path $PSScriptRoot "..\PixelPoach.gba"
if (-not (Test-Path $rom)) { Write-Output "no ROM built"; exit 0 }
$vol = Get-Volume -DriveLetter F -ErrorAction SilentlyContinue
if ($null -ne $vol -and $vol.FileSystemLabel -ceq 'GBA') {
    if (-not (Test-Path 'F:\Homebrew')) { New-Item -ItemType Directory 'F:\Homebrew' | Out-Null }
    Copy-Item $rom 'F:\Homebrew\PixelPoach.gba' -Force
    Write-Output "deployed to F:\Homebrew\PixelPoach.gba"
} else {
    Write-Output "flash cart not present (F: label != GBA); skipped"
}
