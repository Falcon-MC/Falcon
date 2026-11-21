#pragma once

#include <cstdint>
#include <unordered_set>

class Level;
class LevelChunk;

// whole chunks lit on the chunk workers; single block changes relit incrementally on the main thread
class LightSystem {
public:
    static constexpr int MAX_LIGHT = 15;

    static void computeHeightmap(LevelChunk &chunk);

    static void updateHeightAt(LevelChunk &chunk, int x, int z);

    static void computeSkyLight(LevelChunk &chunk);

    static void computeBlockLight(LevelChunk &chunk);

    static void onBlockChanged(Level &level, int32_t x, int32_t y, int32_t z);

    // onBlockChanged for a whole column: a batch of changes relights each column once
    static void relightColumn(LevelChunk &chunk, int x, int z);

    static void updateBlockLight(Level &level, std::unordered_set<int64_t> &pending);

    static int64_t packPosition(int32_t x, int32_t y, int32_t z);

    static int32_t calculateSkyLightSubtracted(const Level &level, bool includeWeather = true);

    static float calculateCelestialAngle(int64_t time);

private:
    static void _computeSkyColumn(LevelChunk &chunk, int x, int z);
};
