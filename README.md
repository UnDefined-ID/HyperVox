# HyperVox — A C++ Minecraft clone built around performance 

## setup
1. Run `install_and_build_linux.sh` and `build-steel.sh` (Linux) or `install_and_build_windows.bat` (Windows, run as admin)(wont work till steel is compiled to c++ in a later update)
2. `./build/minecraft`

Manual: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build`

## Controls
| Key | Action |
|-----|--------|
| WASD | Move |
| Space | Jump / Swim up / Fly up |
| Shift | Sprint / Sneak / Fly down |
| E | Inventory |
| 1-9 / scroll | Hotbar |
| Left click | Break block |
| Right click | Place block |
| Middle click | Pick block (creative) |
| T | Open chat |
| / | Open chat with `/` |
| Esc | Pause / close menu |
| F3 | Toggle debug info |

## Worlds
Saved in `worlds/<name>/` as real **Minecraft 26.3** Anvil files (`level.dat` + `region/*.mca`).  
Worlds are openable in Minecraft — only edited chunks are written; terrain elsewhere is regenerated.

## Data folder
- **Linux:** `~/.local/share/FastVoxelVK/`
- **Windows:** `%APPDATA%\FastVoxelVK\`
- **Override:** set `FASTVOXEL_HOME` environment variable


## Terrain
Terrain comes from SteelMC (vanilla-exact) through the helper in `steel-provider/` (AGPL-3.0). `build-steel.sh` (run automatically by the CMake build, needs internet) builds it into `steel/`. If it is missing the game makes a flat world. 
