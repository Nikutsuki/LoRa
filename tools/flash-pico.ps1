<#
.SYNOPSIS
    Builds and flashes the LoRa firmware onto a Raspberry Pi Pico.

.DESCRIPTION
    Builds the pico_hw_test target, waits for the board to appear as a mass
    storage device in BOOTSEL mode, and copies the UF2 onto it. The board
    reboots into the new firmware by itself once the copy finishes.

    When run with no arguments it configures the build directory if needed,
    builds, and then flashes.

.PARAMETER BuildDir
    CMake build directory. Defaults to <repo>/build.

.PARAMETER Target
    CMake target to build. Defaults to pico_lora_messenger.

.PARAMETER WaitSeconds
    How long to wait for the RPI-RP2 drive to appear.

.PARAMETER SkipBuild
    Flash the existing UF2 without rebuilding.

.PARAMETER Reconfigure
    Delete the build directory and configure from scratch.

.EXAMPLE
    .\tools\flash-pico.ps1
    Build and flash with defaults.

.EXAMPLE
    .\tools\flash-pico.ps1 -SkipBuild
    Flash the last built UF2 without rebuilding.
#>
param(
    [string]$BuildDir,
    [string]$Target = "pico_lora_messenger",
    [int]$WaitSeconds = 60,
    [switch]$SkipBuild,
    [switch]$Reconfigure
)

$ErrorActionPreference = "Stop"

# CMake and Ninja write progress messages to stderr, which PowerShell would treat
# as terminating errors under "Stop". Run native tools without that conversion and
# check their exit codes explicitly instead.
function Invoke-Native {
    param(
        [Parameter(Mandatory = $true)][string]$Exe,
        [Parameter(ValueFromRemainingArguments = $true)][string[]]$Arguments
    )

    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $Exe @Arguments 2>&1 | ForEach-Object { Write-Host $_ }
        return $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previous
    }
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..")

if (-not $BuildDir) {
    $BuildDir = Join-Path $RepoRoot "build"
}

function Resolve-Toolchain {
    # The Arm toolchain is not always on PATH, and CMake needs it to find a
    # compiler, so add the winget install location when it is missing.
    if (Get-Command arm-none-eabi-gcc -ErrorAction SilentlyContinue) {
        return
    }

    $candidates = @(
        "C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin",
        "C:\Program Files\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path (Join-Path $candidate "arm-none-eabi-gcc.exe")) {
            $env:PATH = "$candidate;$env:PATH"
            Write-Host "Added toolchain to PATH: $candidate"
            return
        }
    }

    throw "arm-none-eabi-gcc was not found. Install the Arm GNU Toolchain 14.2.Rel1 or add it to PATH."
}

function Resolve-PicoSdk {
    if (-not $env:PICO_SDK_PATH) {
        $sdk = Join-Path $env:USERPROFILE ".pico-sdk\2.1.1"
        if (Test-Path (Join-Path $sdk "pico_sdk_init.cmake")) {
            $env:PICO_SDK_PATH = $sdk
            Write-Host "Using PICO_SDK_PATH: $sdk"
        } else {
            throw "Pico SDK not found. Set PICO_SDK_PATH or install the SDK to $sdk."
        }
    }

    if (-not $env:PICOTOOL_FETCH_FROM_GIT_PATH) {
        $env:PICOTOOL_FETCH_FROM_GIT_PATH = Join-Path $env:USERPROFILE ".pico-sdk"
    }
}

function Invoke-Build {
    Resolve-Toolchain
    Resolve-PicoSdk

    if ($Reconfigure -and (Test-Path $BuildDir)) {
        Write-Host "Removing build directory for a clean configure..."
        Remove-Item $BuildDir -Recurse -Force
    }

    if (-not (Test-Path (Join-Path $BuildDir "CMakeCache.txt"))) {
        Write-Host "Configuring..."
        $code = Invoke-Native cmake -S $RepoRoot -B $BuildDir -G Ninja -DPICO_BOARD=pico -DCMAKE_BUILD_TYPE=Release
        if ($code -ne 0) {
            throw "CMake configure failed with exit code $code."
        }
    }

    Write-Host "Building $Target..."
    $code = Invoke-Native cmake --build $BuildDir --target $Target
    if ($code -ne 0) {
        throw "Build failed with exit code $code."
    }
}

function Find-Uf2 {
    # The firmware is built at the top of the build tree, so look there first and
    # fall back to a recursive search for older layouts.
    $uf2 = Join-Path $BuildDir "$Target.uf2"
    if (Test-Path $uf2) {
        return $uf2
    }

    $found = Get-ChildItem -Path $BuildDir -Recurse -Filter "$Target.uf2" -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $found) {
        throw "Could not find $Target.uf2 under $BuildDir. Build first, or pass -Target."
    }
    return $found.FullName
}

function Find-BootloaderDrive {
    param([int]$TimeoutSeconds)

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        foreach ($drive in [System.IO.DriveInfo]::GetDrives()) {
            try {
                if ($drive.IsReady -and $drive.VolumeLabel -eq "RPI-RP2") {
                    return $drive.RootDirectory.FullName
                }
            }
            catch {
                continue
            }
        }
        Start-Sleep -Milliseconds 500
    }
    return $null
}

if (-not $SkipBuild) {
    Push-Location $RepoRoot
    try {
        Invoke-Build
    }
    finally {
        Pop-Location
    }
}

$Uf2Path = Find-Uf2
$sizeKb = [math]::Round((Get-Item $Uf2Path).Length / 1KB)
Write-Host "Firmware: $Uf2Path ($sizeKb KB)"

$picoRoot = Find-BootloaderDrive -TimeoutSeconds $WaitSeconds
if (-not $picoRoot) {
    Write-Host ""
    Write-Host "No RPI-RP2 drive found after $WaitSeconds seconds." -ForegroundColor Yellow
    Write-Host "Unplug the Pico, hold BOOTSEL, plug it back in, then release BOOTSEL."
    throw "Timed out waiting for the Pico bootloader."
}

$destination = Join-Path $picoRoot (Split-Path -Leaf $Uf2Path)
Write-Host "Copying to $destination ..."
Copy-Item -LiteralPath $Uf2Path -Destination $destination -Force

Write-Host "Done. The Pico reboots into the new firmware automatically." -ForegroundColor Green
