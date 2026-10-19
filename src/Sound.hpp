#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <unordered_map>

// Plays .ogg/.wav files from minecraft-sounds.zip (or a pre-extracted folder).
// Uses OpenAL if available at runtime; falls back to a silent stub gracefully.
// All public methods are thread-safe and no-op when audio is unavailable.
class Sound {
public:
    static Sound& get();
    bool init(const std::filesystem::path& soundsRoot);   // returns false = no audio (silent)
    void shutdown();

    // category multipliers (0..1)
    float volMaster=1.f, volBlock=1.f, volAmbient=1.f, volPlayer=1.f;

    // play a Minecraft sound event (e.g. "block.stone.break", "player.jump")
    void play(const std::string& event, float vol=1.f, float pitch=1.f);
    // ambient: looping sounds (water, etc.) — call every frame while in the region
    void loop(const std::string& event, float vol=1.f);
    void stopLoop(const std::string& event);

    bool available() const { return m_ok; }
    const std::filesystem::path& root() const { return m_root; }

private:
    Sound() = default;
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::filesystem::path m_root;
    bool m_ok = false;
};
