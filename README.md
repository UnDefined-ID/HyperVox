# FastVoxelVK — Minecraft-style Vulkan voxel sandbox

## Quick start
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
| F | Toggle flight (creative) |
| E | Inventory |
| 1-9 / scroll | Hotbar |
| Left click | Break block |
| Right click | Place block |
| Middle click | Pick block (creative) |
| T | Open chat |
| / | Open chat with `/` |
| Esc | Pause / close menu |
| F3 | Toggle debug info |

## Chat commands
| Command | Description |
|---------|-------------|
| `/tp <x> <y> <z>` | Teleport |
| `/gamemode <survival\|creative>` | Switch game mode |
| `/gamerule [rule] [value]` | tickRate, doDaylightCycle, doWeatherCycle |
| `/time <set\|add\|query> <value\|day\|night\|...>` | World time |
| `/tick <rate\|freeze\|unfreeze\|step>` | Tick rate control |
| `/give <item> [count]` | Add item to inventory |
| `/setblock <x> <y> <z> <block>` | Place a block |
| `/clear` | Clear inventory |
| `/kill` | Kill player |
| `/seed` | Show world seed |
| `/fly` | Toggle flight |
| `/help` | List all commands |

## Worlds
Saved in `worlds/<name>/` as real **Minecraft 1.21.11** Anvil files (`level.dat` + `region/*.mca`).  
Worlds are openable in Minecraft — only edited chunks are written; terrain elsewhere is regenerated.

## Resource packs
Drop a pack folder into `<data dir>/resourcepacks/` then Options → Textures. Works with any 1.21.x pack.

## Data folder
- **Linux:** `~/.local/share/FastVoxelVK/`
- **Windows:** `%APPDATA%\FastVoxelVK\`
- **Override:** set `FASTVOXEL_HOME` environment variable

## What's included
- Infinite streaming world (closest-first threaded meshing, no render distance cap in code, GPU limited)
- Break & place blocks, survival inventory, 7-tab creative menu (~330 blocks, ~200 items)
- Real Minecraft panorama title screen
- Full settings persistence (options.txt, auto-saved)
- Atlas disk cache (block atlas + GUI atlas + panorama cached after first build)
- Optional audio via OpenAL + libvorbisfile (dlopen — no hard dependency; silent if absent)
- Water swimming/buoyancy with jump-out
- All singleplayer chat commands
