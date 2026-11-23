Terrain: SteelMC (vanilla-exact)
================================
Run ./build-steel.sh once (the normal CMake build runs it for you).  It installs Rust if needed, downloads the Minecraft data jar,
builds steel-provider (source in ../steel-provider, AGPL-3.0 like SteelMC) and writes steel-provider + block.json into this folder.
Without them the game generates a flat world.  The helper runs as a separate local process and is stopped when the game quits.
Linux / macOS only for now.