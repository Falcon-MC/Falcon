#pragma once

#include <cstdint>
#include <random>

class LevelChunk;

// the nether_cave_carver_feature every nether biome places in its pregeneration_pass
class NetherCarvers {
public:
    explicit NetherCarvers(int64_t seed);

    void carve(LevelChunk &chunk) const;

private:
    struct Position {
        float mX;
        float mY;
        float mZ;
    };

    static void _addCaves(std::mt19937 &random, int32_t sourceX, int32_t sourceZ, LevelChunk &chunk);

    static void _addTunnel(std::mt19937 &parent, LevelChunk &chunk, Position position, float thickness, float yaw,
                           float pitch, int32_t start, int32_t length, float yScale);

    // false when the step met lava and carved nothing
    static bool _carveStep(LevelChunk &chunk, const Position &position, float radius, float yScale);

    uint32_t mSeed;
};
