#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$(readlink -f "$0")")"

if [[ "$(id -u)" -eq 0 ]]; then SUDO=""; else SUDO="sudo"; fi

install_pkgs() {
  if command -v apt-get >/dev/null 2>&1; then
    $SUDO apt-get update
    $SUDO apt-get install -y build-essential cmake ninja-build pkg-config libvulkan-dev vulkan-tools libglfw3-dev libpng-dev glslc libopenal-dev libvorbisfile3 libvorbis-dev 2>/dev/null || true
  elif command -v dnf >/dev/null 2>&1; then
    $SUDO dnf install -y gcc-c++ cmake ninja-build pkgconf-pkg-config vulkan-loader-devel vulkan-headers glfw-devel libpng-devel glslc openal-soft-devel libvorbis-devel 2>/dev/null || true
  elif command -v pacman >/dev/null 2>&1; then
    $SUDO pacman -Sy --needed --noconfirm base-devel cmake ninja pkgconf vulkan-headers vulkan-loader glfw-x11 libpng shaderc openal libvorbis 2>/dev/null || true
  else
    echo "Unsupported Linux package manager. Install CMake, a C++20 compiler, Vulkan headers/loader, GLFW3, libpng, Ninja, and glslc manually."
    exit 1
  fi
}

need=0
for c in cmake c++ ninja pkg-config; do
  command -v "$c" >/dev/null 2>&1 || need=1
done
if ! command -v glslc >/dev/null 2>&1 && ! command -v glslangValidator >/dev/null 2>&1; then need=1; fi
if [[ ! -f /usr/include/vulkan/vulkan.h && ! -f /usr/local/include/vulkan/vulkan.h ]]; then need=1; fi
if ! pkg-config --exists glfw3 2>/dev/null; then need=1; fi
if ! pkg-config --exists libpng 2>/dev/null; then need=1; fi
if [[ "$need" -eq 1 ]]; then install_pkgs; fi

rm -f build.log configure.log
rm -rf build

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release 2>&1 | tee configure.log
cmake --build build --parallel "$(nproc 2>/dev/null || echo 2)" 2>&1 | tee build.log

echo
echo "Build complete: $PWD/build/minecraft"
echo "Run with: ./build/minecraft"
