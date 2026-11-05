#pragma once

#include <cstdint>
#include <random>
#include <vector>

class LevelChunk;
class OverworldAquifer;

// caves and canyons cut through a chunk once its surface is built (sub_14CDD16A0)
class OverworldCarvers {
public:
    explicit OverworldCarvers(int64_t seed);

    void carve(LevelChunk &chunk, OverworldAquifer &aquifer) const;

private:
    // a minecraft:cave_carver_feature from the behavior packs
    struct CaveSettings {
        int32_t mSkipCarveChance;
        int32_t mHeightLimit;
        float mYScale[2];
        float mHorizontalRadiusMultiplier[2];
        float mVerticalRadiusMultiplier[2];
        float mFloorLevel[2];
    };

    // one carving step; y is counted from the bottom of the world
    struct CaveSegment {
        float mX;
        float mY;
        float mZ;
        float mHorizontalRadius;
        float mVerticalRadius;
        float mFloorLevel;
        int32_t mStep;
        int32_t mLength;
        float mThickness;
    };

    void _carveCaves(const CaveSettings &settings, LevelChunk &chunk, OverworldAquifer &aquifer) const;

    static void _addCaves(const CaveSettings &settings, std::mt19937 &random, int32_t sourceX, int32_t sourceZ,
                          std::vector<CaveSegment> &segments);

    static void _addRoom(const CaveSettings &settings, std::mt19937 &random, float x, float y, float z,
                         const float *shape, std::vector<CaveSegment> &segments);

    static void _addTunnel(std::mt19937 &parent, float x, float y, float z, float thickness, float yaw, float pitch,
                           int32_t step, int32_t length, float yScale, const float *shape,
                           std::vector<CaveSegment> &segments);

    static void _carveSegment(const CaveSegment &segment, int32_t heightLimit, LevelChunk &chunk,
                              OverworldAquifer &aquifer);

    void _carveCanyons(LevelChunk &chunk, OverworldAquifer &aquifer) const;

    static void _addCanyon(std::mt19937 &random, int32_t sourceX, int32_t sourceZ, LevelChunk &chunk,
                           OverworldAquifer &aquifer);

    uint32_t mSeed;
};
