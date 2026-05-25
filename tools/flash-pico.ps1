param(
    [string]$Target = "pico_ping_pong",
    [string]$BuildDir,
    [string]$Uf2Path,
    [int]$WaitSeconds = 30,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..")

if (-not $BuildDir) {
    $BuildDir = Join-Path $RepoRoot "build"
}

if (-not $SkipBuild) {
    Write-Host "Building $Target..."
    Push-Location $RepoRoot
    try {
        cmake --build $BuildDir --target $Target
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed with exit code $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}

if (-not $Uf2Path) {
    $Uf2 = Get-ChildItem -Path $BuildDir -Recurse -Filter "$Target.uf2" |
        Select-Object -First 1

    if (-not $Uf2) {
        throw "Could not find $Target.uf2 under $BuildDir."
    }

    $Uf2Path = $Uf2.FullName
}

if (-not (Test-Path -LiteralPath $Uf2Path)) {
    throw "UF2 file does not exist: $Uf2Path"
}

Write-Host "Waiting for Pico bootloader drive named RPI-RP2..."
Write-Host "If it is not connected, unplug it, hold BOOTSEL, plug it in, then release BOOTSEL."

$Deadline = (Get-Date).AddSeconds($WaitSeconds)
$PicoRoot = $null

while ((Get-Date) -lt $Deadline) {
    foreach ($Drive in [System.IO.DriveInfo]::GetDrives()) {
        try {
            if ($Drive.IsReady -and $Drive.VolumeLabel -eq "RPI-RP2") {
                $PicoRoot = $Drive.RootDirectory.FullName
                break
            }
        }
        catch {
            continue
        }
    }

    if ($PicoRoot) {
        break
    }

    Start-Sleep -Milliseconds 500
}

if (-not $PicoRoot) {
    throw "Timed out waiting for RPI-RP2. Re-run this script with the Pico in BOOTSEL mode."
}

$Destination = Join-Path $PicoRoot (Split-Path -Leaf $Uf2Path)
Write-Host "Copying $Uf2Path to $Destination..."
Copy-Item -LiteralPath $Uf2Path -Destination $Destination -Force

Write-Host "Done. The Pico should reboot and run the new firmware."
