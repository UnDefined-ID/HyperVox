# HyperVox — Minecraft clone built around performance 

## setup
1. Put `minecraft-sounds.zip` into `assets/` (optional — audio)
2. Run `install_and_build_linux.sh` (Linux) or `install_and_build_windows.bat` (Windows, run as admin)
3. `./build/minecraft`

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
Saved in `worlds/<name>/` as real **Minecraft 1.21.11** Anvil files (`level.dat` + `region/*.mca`).  
Worlds are openable in Minecraft — only edited chunks are written; terrain elsewhere is regenerated.

## Resource packs
Drop a pack folder into `<data dir>/resourcepacks/` then Options → Textures. Works with any 1.21.x pack.

## Data folder
- **Linux:** `~/.local/share/FastVoxelVK/`
- **Windows:** `%APPDATA%\FastVoxelVK\`
- **Override:** set `FASTVOXEL_HOME` environment variable
