<#
.SYNOPSIS
  One-shot setup for ConsoleDemo_FPN: installs vcpkg + OpenCV + ONNX Runtime,
  integrates with MSBuild, and verifies the Sphinx SDK placement.

.DESCRIPTION
  Idempotent. Re-running is safe. Requires:
    - Git
    - Visual Studio 2022 with the "Desktop development with C++" workload
      (v143 toolset) and Windows 10 SDK
    - PowerShell 5+ (preinstalled on Win10/11)

  The Sphinx SDK is vendor-only and cannot be auto-installed. The script
  checks for a sibling ..\SphinxLib directory and tells you what's missing.

.PARAMETER VcpkgRoot
  Where to install vcpkg. Defaults to C:\vcpkg, or $env:VCPKG_ROOT if set.

.PARAMETER SkipVcpkg
  Skip vcpkg setup entirely (use if you've wired your own OpenCV/ONNX paths).

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\setup.ps1
#>

[CmdletBinding()]
param(
    [string]$VcpkgRoot = $(if ($env:VCPKG_ROOT) { $env:VCPKG_ROOT } else { 'C:\vcpkg' }),
    [switch]$SkipVcpkg
)

$ErrorActionPreference = 'Stop'
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
Write-Host "Repo root: $repoRoot" -ForegroundColor Cyan

function Require-Cmd([string]$name) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
        throw "Required command '$name' not found in PATH."
    }
}

function Find-MsBuild {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found. Install Visual Studio 2022."
    }
    $vsRoot = & $vswhere -latest -products * `
        -requires Microsoft.Component.MSBuild `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $vsRoot) {
        throw "Visual Studio with C++ tools not found. Install the 'Desktop development with C++' workload."
    }
    $msb = Join-Path $vsRoot 'MSBuild\Current\Bin\MSBuild.exe'
    if (-not (Test-Path $msb)) { throw "MSBuild.exe not found under $vsRoot" }
    return $msb
}

# --- 1. Prerequisite checks ---------------------------------------------------
Write-Host "`n[1/4] Checking prerequisites..." -ForegroundColor Cyan
Require-Cmd git
$msbuild = Find-MsBuild
Write-Host "  git:     $((Get-Command git).Source)"
Write-Host "  msbuild: $msbuild"

# --- 2. Sphinx SDK presence check --------------------------------------------
Write-Host "`n[2/4] Checking Sphinx SDK..." -ForegroundColor Cyan
$sphinxDir = Join-Path (Split-Path $repoRoot -Parent) 'SphinxLib'
$sphinxOk = $true
foreach ($f in @('SphinxLib.h', 'Debug64\SphinxLib.lib', 'Release64\SphinxLib.lib')) {
    $p = Join-Path $sphinxDir $f
    if (-not (Test-Path $p)) { Write-Warning "  missing: $p"; $sphinxOk = $false }
    else { Write-Host "  ok: $p" }
}
if (-not $sphinxOk) {
    Write-Warning "Sphinx SDK not fully installed at $sphinxDir."
    Write-Warning "Obtain it from MRC Systems and place it so the layout is:"
    Write-Warning "  <parent>\SphinxSDK-demo\  (this repo)"
    Write-Warning "  <parent>\SphinxLib\       (SphinxLib.h, Debug64\, Release64\)"
}

# --- 3. vcpkg install + integrate --------------------------------------------
if ($SkipVcpkg) {
    Write-Host "`n[3/4] Skipping vcpkg (per -SkipVcpkg)." -ForegroundColor Yellow
} else {
    Write-Host "`n[3/4] vcpkg at $VcpkgRoot ..." -ForegroundColor Cyan
    if (-not (Test-Path $VcpkgRoot)) {
        Write-Host "  cloning vcpkg..."
        git clone https://github.com/microsoft/vcpkg.git $VcpkgRoot
    } else {
        Write-Host "  vcpkg already present, pulling latest..."
        Push-Location $VcpkgRoot
        git pull --ff-only
        Pop-Location
    }
    $bootstrap = Join-Path $VcpkgRoot 'bootstrap-vcpkg.bat'
    if (-not (Test-Path (Join-Path $VcpkgRoot 'vcpkg.exe'))) {
        Write-Host "  bootstrapping..."
        & $bootstrap -disableMetrics
    }
    $vcpkgExe = Join-Path $VcpkgRoot 'vcpkg.exe'
    Write-Host "  integrating with MSBuild..."
    & $vcpkgExe integrate install | Out-Null
    Write-Host "  pre-warming opencv4 + onnxruntime (this can take a while)..."
    & $vcpkgExe install opencv4:x64-windows onnxruntime:x64-windows
    [System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', $VcpkgRoot, 'User')
    Write-Host "  set VCPKG_ROOT=$VcpkgRoot for current user."
}

# --- 4. Done ------------------------------------------------------------------
Write-Host "`n[4/4] Setup complete." -ForegroundColor Green
Write-Host "Build with:"
Write-Host "  scripts\build.bat" -ForegroundColor Yellow
Write-Host "or directly:"
Write-Host "  `"$msbuild`" ConsoleDemo_FPN\ConsoleDemo.sln /p:Configuration=Release /p:Platform=x64" -ForegroundColor Yellow
if (-not $sphinxOk) {
    Write-Warning "Build will fail until SphinxLib is in place (see warnings above)."
}
