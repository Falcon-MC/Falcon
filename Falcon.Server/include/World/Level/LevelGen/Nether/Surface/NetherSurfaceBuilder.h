#pragma once

#include "World/Level/LevelGen/End/Noise/TheEndOctaveNoise.h"
#include "World/Level/LevelGen/Overworld/Surface/SurfaceSimplexNoise.h"

#include <cstdint>
#include <map>
#include <memory>
#include <random>
#include <vector>

class BlockState;
class LevelChunk;

// one column's inputs, shared by the two kinds of biome surface
struct NetherSurfaceColumn {
    LevelChunk &mChunk;
    std::mt19937 &mRandom;
    int32_t mX;
    int32_t mZ;
    float mDepthNoise;
};

class NetherBiomeSurface {
public:
    virtual ~NetherBiomeSurface() = default;

    virtual void build(const NetherSurfaceColumn &column) const = 0;
};

// the floor, the ceiling and the beach of the biome definitions, each material with its own noise
class CappedNetherSurface : public NetherBiomeSurface {
public:
    struct Material {
        const BlockState *mBlock;
        std::unique_ptr<TheEndOctaveNoise> mNoise;
    };

    CappedNetherSurface(uint32_t seed, const std::vector<const BlockState *> &floor,
                        const std::vector<const BlockState *> &ceiling, const BlockState *beach,
                        const TheEndOctaveNoise &beachNoise);

    void build(const NetherSurfaceColumn &column) const override;

private:
    std::vector<Material> mFloor;
    std::vector<Material> mCeiling;
    const BlockState *mBeach;
    const TheEndOctaveNoise &mBeachNoise;
};

// a top and a mid layer over the foundation, with height bound overrides picked by noise
class LayeredNetherSurface : public NetherBiomeSurface {
public:
    struct Adjustment {
        float mFrequency;
        float mLowerBound;
        float mUpperBound;
        int32_t mMinY;
        int32_t mMaxY;
        const BlockState *mTop;
        const BlockState *mMid;
        const BlockState *mSeaFloor;
    };

    LayeredNetherSurface(const std::vector<SurfaceSimplexNoise> &adjustmentNoise, const BlockState &top,
                         const BlockState &mid, const BlockState &seaFloor, int32_t seaFloorDepth,
                         std::vector<Adjustment> adjustments);

    void build(const NetherSurfaceColumn &column) const override;

private:
    float _adjustmentValue(float frequency, int32_t worldX, int32_t worldZ) const;

    const std::vector<SurfaceSimplexNoise> &mAdjustmentNoise;
    const BlockState *mTop;
    const BlockState *mMid;
    const BlockState *mSeaFloor;
    int32_t mSeaFloorDepth;
    std::vector<Adjustment> mAdjustments;
};

class NetherSurfaceBuilder {
public:
    NetherSurfaceBuilder(uint32_t seed, const std::vector<SurfaceSimplexNoise> &depthNoise,
                         const std::vector<SurfaceSimplexNoise> &adjustmentNoise);

    // the bedrock floor, the bedrock ceiling, then every column, all from one random seeded by the chunk
    void apply(LevelChunk &chunk) const;

private:
    void _fillDepthNoise(int32_t chunkX, int32_t chunkZ, float *noise) const;

    const std::vector<SurfaceSimplexNoise> &mDepthNoise;
    std::unique_ptr<TheEndOctaveNoise> mBeachNoise;
    std::map<int32_t, std::unique_ptr<NetherBiomeSurface>> mSurfaces;
};
