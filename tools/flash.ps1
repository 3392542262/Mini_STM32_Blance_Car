param(
    # Override with OPENOCD_INTERFACE, e.g. interface/stlink.cfg
    [string]$Interface = $(if ($env:OPENOCD_INTERFACE) { $env:OPENOCD_INTERFACE } else { 'interface/cmsis-dap.cfg' }),
    [string]$Target = 'target/stm32f1x.cfg',
    [string]$Elf = 'build/Debug/Mini_STM32_Blance_Car.elf',
    [int]$Speed = 1000
)

# Flash the ELF with OpenOCD, verify and reset.

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

$openocd = Find-Tool 'openocd' @(
    "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\xpack-dev-tools.openocd-xpack_*\xpack-openocd-*\bin\openocd.exe",
    "$env:ProgramFiles\OpenOCD*\bin\openocd.exe",
    "${env:ProgramFiles(x86)}\OpenOCD*\bin\openocd.exe",
    "$env:USERPROFILE\.eide\tools\openocd*\bin\openocd.exe"
)

if (-not $openocd) {
    Write-Host 'OpenOCD was not found.' -ForegroundColor Red
    Write-Host 'Install it with: winget install xpack-dev-tools.openocd-xpack'
    exit 1
}

if (-not (Test-Path $Elf)) {
    Write-Host "Cannot find $Elf, run Build first." -ForegroundColor Red
    exit 1
}

$openocdArgs = @(
    '-f', $Interface,
    '-c', 'transport select swd',
    '-f', $Target,
    '-c', "adapter speed $Speed",
    '-c', "program $Elf verify reset exit"
)

Write-Host "Flashing: $Elf" -ForegroundColor Cyan
Write-Host "Interface: $Interface" -ForegroundColor Cyan

# Use the .NET process API so OpenOCD log output on stderr is not treated
# as a PowerShell error (Windows PowerShell 5.1 behaviour).
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $openocd
$psi.Arguments = ($openocdArgs | ForEach-Object {
    if ($_ -match '\s') { '"{0}"' -f $_ } else { $_ }
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

exit $process.ExitCode
