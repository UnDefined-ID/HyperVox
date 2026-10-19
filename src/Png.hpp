#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

struct AtlasImage { uint32_t width=0,height=0; std::vector<uint8_t> rgba; };

// Reads any PNG (palette / gray / gray+alpha / RGB / RGBA, 1-16 bit) into 8-bit RGBA.
bool readPng(const std::filesystem::path& p, AtlasImage& out);
