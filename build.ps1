<#
.SYNOPSIS
    Configure + build MainMenuToggles and stage a mod-manager-ready folder in .\dist.
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\build.ps1
    powershell -ExecutionPolicy Bypass -File .\build.ps1 -Preset debug -Zip
    powershell -ExecutionPolicy Bypass -File .\build.ps1 -Lint
#>
param(
    [ValidateSet('release', 'debug')] [string] $Preset = 'release',
    [switch] $Zip,
    [switch] $Lint
)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

# Visual Studio developer environment (cl, ninja, cmake, bundled vcpkg).
$installer = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer"
$env:PATH = "$installer;$env:PATH"  # VsDevCmd itself calls vswhere.exe
$vs = & "$installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio with the C++ workload was not found.' }
Import-Module "$vs\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null

# Prefer the CMake/Ninja/Git that ship with Visual Studio.
$ide = "$vs\Common7\IDE\CommonExtensions\Microsoft"
$env:PATH = "$ide\CMake\CMake\bin;$ide\CMake\Ninja;$ide\TeamFoundation\Team Explorer\Git\cmd;$env:PATH"

if (-not $env:VCPKG_ROOT) {
    $env:VCPKG_ROOT = "$vs\VC\vcpkg"
    # Let VS's bundled vcpkg use VS's CMake/Ninja instead of downloading its own.
    $env:VCPKG_FORCE_SYSTEM_BINARIES = '1'
}
Write-Host "Using vcpkg at $env:VCPKG_ROOT"

if (-not (Test-Path 'extern\CommonLibSSE-NG\CMakeLists.txt')) {
    git submodule update --init --recursive
    if ($LASTEXITCODE) { throw 'git submodule update failed' }
}

# -Lint compiles the plugin with MSVC Code Analysis; a normal build switches it back off.
$analyze = if ($Lint) { 'ON' } else { 'OFF' }
cmake --preset $Preset "-DMMT_ANALYZE=$analyze"
if ($LASTEXITCODE) { throw 'CMake configure failed' }
if ($Lint) {
    # Analysis only reports while compiling, so recompile the plugin's sources every time.
    Remove-Item "build\$Preset\CMakeFiles\MainMenuToggles.dir\src\*.obj" -ErrorAction SilentlyContinue
}
cmake --build --preset $Preset | Tee-Object -Variable buildOutput
if ($LASTEXITCODE) { throw 'Build failed' }

Write-Host "`nMod folder ready: $PSScriptRoot\dist"
Get-ChildItem dist -Recurse -File | ForEach-Object { Write-Host "  $($_.FullName.Substring($PSScriptRoot.Length + 1))" }

if ($Lint) {
    $analysis = @($buildOutput | Select-String -Pattern 'warning C(6|26|28)\d{3}' | ForEach-Object { $_.Line.Trim() } | Sort-Object -Unique)
    Write-Host "`nMSVC Code Analysis: $($analysis.Count) warning(s)"
    $analysis | ForEach-Object { Write-Host $_ }

    $tidy = "$vs\VC\Tools\Llvm\x64\bin\clang-tidy.exe"
    if (-not (Test-Path $tidy)) { throw 'clang-tidy.exe was not found (VS C++ Clang tools component).' }
    Write-Host "`nclang-tidy (checks in .clang-tidy):"
    foreach ($file in Get-ChildItem src -Filter *.cpp) {
        # /Y- makes clang parse PCH.h directly, since it can't read MSVC's precompiled header; clang doesn't know the /analyze flags.
        & $tidy --quiet -p "build\$Preset" --extra-arg=/Y- --removed-arg=/analyze --removed-arg=/analyze:external- $file.FullName
    }
}

if ($Zip) {
    $match = (Select-String -Path CMakeLists.txt -Pattern 'VERSION (\d+)\.(\d+)\.\d+').Matches[0]
    $version = "$($match.Groups[1].Value).$($match.Groups[2].Value)"
    # MO2 reads the mod version from fomod\info.xml, otherwise it falls back to the file date.
    $stage = "$PSScriptRoot\build\package"
    Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force "$stage\fomod" | Out-Null
    Copy-Item "$PSScriptRoot\dist\*" $stage -Recurse
    Copy-Item "$PSScriptRoot\fomod\ModuleConfig.xml" "$stage\fomod"
    @"
<?xml version="1.0" encoding="UTF-8"?>
<fomod>
	<Name>Configurable Main Menu</Name>
	<Author>hankinsrim</Author>
	<Version>$version</Version>
	<Website>https://github.com/hankinsrim/ConfigurableMainMenu</Website>
	<Description>Choose which parts of the Skyrim main menu are visible.</Description>
</fomod>
"@ | Out-File "$stage\fomod\info.xml" -Encoding utf8
    $zipPath = "$PSScriptRoot\build\ConfigurableMainMenu-$version.zip"
    Compress-Archive -Path "$stage\*" -DestinationPath $zipPath -Force
    Write-Host "Zip: $zipPath"
}
