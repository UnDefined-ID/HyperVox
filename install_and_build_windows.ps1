$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

function Require-Command($name) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
        throw "Required command '$name' was not found. Run this script from an Administrator PowerShell with winget available."
    }
}

Require-Command winget

# Install the build toolchain and Vulkan SDK if they are not already present.
$packages = @(
    @{ Id = 'Git.Git'; Name = 'Git' },
    @{ Id = 'Kitware.CMake'; Name = 'CMake' },
    @{ Id = 'Ninja-build.Ninja'; Name = 'Ninja' },
    @{ Id = 'Microsoft.VisualStudio.2022.BuildTools'; Name = 'Visual Studio Build Tools' },
    @{ Id = 'KhronosGroup.VulkanSDK'; Name = 'Vulkan SDK' }
)
foreach ($p in $packages) {
    winget install --id $p.Id --exact --accept-source-agreements --accept-package-agreements --silent
}

# Refresh PATH after winget installations.
$env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
Require-Command git
Require-Command cmake
Require-Command ninja

$vcpkg = Join-Path $PSScriptRoot 'vcpkg'
if (-not (Test-Path (Join-Path $vcpkg 'vcpkg.exe'))) {
    if (Test-Path $vcpkg) { Remove-Item -Recurse -Force $vcpkg }
    git clone https://github.com/microsoft/vcpkg.git $vcpkg
    Push-Location $vcpkg
    & .\bootstrap-vcpkg.bat
    Pop-Location
}

$toolchain = Join-Path $vcpkg 'scripts\buildsystems\vcpkg.cmake'
$env:VCPKG_DEFAULT_TRIPLET = 'x64-windows'

if (Test-Path build) { Remove-Item -Recurse -Force build }
cmake -S . -B build -G Ninja `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" `
    -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --parallel --verbose

Write-Host ""
Write-Host "Build complete: $PSScriptRoot\build\minecraft.exe"
Write-Host "Run: .\build\minecraft.exe"
