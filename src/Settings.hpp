#pragma once
#include <filesystem>
#include <string>
#include <vector>

enum Action{ACT_FWD,ACT_BACK,ACT_LEFT,ACT_RIGHT,ACT_JUMP,ACT_DOWN,ACT_SPRINT,ACT_FLY,ACT_INVENTORY,ACT_COUNT};

// User options.  Stored as plain "key:value" lines in <data dir>/options.txt and written every time a value changes.
struct Settings{
 int renderDistance=12;        // chunks; no upper limit besides GPU memory (the slider goes to kMaxSlider)
 int fov=70;                   // degrees
 float mouseSens=0.5f;         // 0..1
 bool vsync=false;
 bool showDebug=true;
 std::string resourcePack;     // folder name inside <data dir>/resourcepacks, empty = default pack
 int keys[ACT_COUNT]{};        // GLFW key codes
 int cleanerMode=0;              // memory cleaner: 0 normal, 1 aggressive, 2 very aggressive
 bool mipmaps=true;            // mip-mapped block textures (restart to apply)
 int ramLimitMB=4096;           // soft memory limit (>=256): above ~85% the game frees memory and pauses chunk loading
 int guiScale=0;               // 0 = auto, otherwise the fixed menu scale (1..6)
 int renderer=0;               // 0 = Vulkan, 1 = OpenGL (applied on the next start; Vulkan falls back to OpenGL automatically)
 std::string username="Player";
 std::string skin="default:wide/steve";   // "default:<wide|slim>/<name>" (texture pack) or "custom:<file>.png" (<data dir>/skins)

 static constexpr int kMinRender=2, kMaxSlider=128, kHardMaxRender=1024;
 Settings(){resetKeys();}
 void resetKeys();
 void clamp();
 bool load();                  // false if the file did not exist (defaults are kept)
 bool save() const;
 static std::filesystem::path dataDir();           // created on first use
 static std::filesystem::path file();
 static std::filesystem::path legacyDataDir();      // old FastVoxelVK folder (migrated automatically)
 static std::vector<std::filesystem::path> packDirs(); // every folder that is searched for resource packs
 static std::filesystem::path skinsDir();
 static std::filesystem::path savesDir();
};
