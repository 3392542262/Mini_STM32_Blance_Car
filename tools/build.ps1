# Build the STM32 project in Debug mode.
# Tools are auto-detected: STM32Cube bundles, PATH, or common install directories.

param(
    [string]$BuildDir = 'build/Debug'
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

function Find-Tool {
    param(
        [string]$Name,
        [string[]]$Patterns = @()
    )

    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }

    foreach ($pattern in $Patterns) {
        $hit = Get-ChildItem $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($hit) {
            return $hit.FullName
        }
    }

    return $null
}

# Run a native tool and forward both streams without PowerShell error wrapping.
function Invoke-Native {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $FilePath
    $psi.Arguments = ($Arguments | ForEach-Object {
        if ($_ -match '[\s"]') { '"{0}"' -f ($_ -replace '"', '\"') } else { $_ }
    }) -join ' '
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    [void]$process.Start()
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()

    if ($stdout) { Write-Host $stdout }
    if ($stderr) { Write-Host $stderr }

    return $process.ExitCode
}

$cmake = Find-Tool 'cmake' @(
    "$env:LOCALAPPDATA\stm32cube\bundles\cmake\*\bin\cmake.exe",
    "$env:ProgramFiles\CMake\bin\cmake.exe",
    "${env:ProgramFiles(x86)}\CMake\bin\cmake.exe"
)

if (-not $cmake) {
    Write-Host 'cmake was not found.' -ForegroundColor Red
    Write-Host 'Install CMake or the STM32 VS Code extension (which bundles CMake).'
    exit 1
}

$ninja = Find-Tool 'ninja' @(
    "$env:LOCALAPPDATA\stm32cube\bundles\ninja\*\bin\ninja.exe",
    "$env:ProgramFiles\Ninja\ninja.exe"
)

if ($ninja) {
    $env:PATH = (Split-Path -Parent $ninja) + ';' + $env:PATH
}

# Make sure the GNU Arm toolchain is available on PATH.
$gcc = Find-Tool 'arm-none-eabi-gcc' @(
    "$env:CUBE_BUNDLE_PATH\gnu-tools-for-stm32\*\bin\arm-none-eabi-gcc.exe",
    "$env:USERPROFILE\.eide\tools\gcc_arm\bin\arm-none-eabi-gcc.exe",
    "$env:ProgramFiles\GNU Arm Embedded Toolchain\*\bin\arm-none-eabi-gcc.exe",
    "${env:ProgramFiles(x86)}\GNU Arm Embedded Toolchain\*\bin\arm-none-eabi-gcc.exe",
    "${env:ProgramFiles(x86)}\Arm GNU Toolchain arm-none-eabi\*\bin\arm-none-eabi-gcc.exe"
)

if (-not $gcc) {
    Write-Host 'arm-none-eabi-gcc was not found.' -ForegroundColor Red
    Write-Host 'Install the GNU Arm Embedded Toolchain or the STM32 VS Code extension.'
    exit 1
}

$env:PATH = (Split-Path -Parent $gcc) + ';' + $env:PATH

$buildDir = Join-Path $root $BuildDir
$cache = Join-Path $buildDir 'CMakeCache.txt'
$toolchain = Join-Path $root 'cmake/gcc-arm-none-eabi.cmake'

if (-not (Test-Path $cache)) {
    Write-Host "First build, configuring: $buildDir" -ForegroundColor Cyan
    $code = Invoke-Native $cmake @(
        '-S', $root,
        '-B', $buildDir,
        '-G', 'Ninja',
        "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
        '-DCMAKE_BUILD_TYPE=Debug'
    )
    if ($code -ne 0) {
        exit $code
    }
}

Write-Host "Building: $buildDir" -ForegroundColor Cyan
exit (Invoke-Native $cmake @('--build', $buildDir))

