#pragma once
// Minecraft 1.18+ climate noise, rebuilt from the world seed (Xoroshiro128++, Perlin octaves, shifted 2D noise) plus the vanilla terrain-offset spline.
// Verified against a real generated world: continentalness / erosion / temperature / humidity / weirdness match the original game.
#include <cstdint>
#include <deque>
#include <memory>
#include <vector>
struct ClimateSample{ float c,e,t,h,w; };      // continentalness, erosion, temperature, humidity (vegetation), weirdness (ridge)
class Climate{
public:
 explicit Climate(uint64_t seed);
 ClimateSample at(int x,int z)const;           // x,z in blocks (vanilla evaluates biome noise at quart = 4 block positions)
 static float offset(float c,float e,float w); // vanilla terrain offset spline (without the -0.50375 base): surface y(MC) ~ 63.5 + 128*offset
 struct Impl; ~Climate();
private:
 std::unique_ptr<Impl> m;
};
