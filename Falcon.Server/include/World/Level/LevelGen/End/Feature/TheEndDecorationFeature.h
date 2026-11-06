#pragma once

#include "World/Item/Item.h"
#include "World/Level/LevelGen/End/Noise/TheEndSimplexNoise.h"
#include "World/Level/LevelGen/Feature/IFeature.h"

#include <cstdint>
#include <random>
#include <vector>

// everything the end places after its terrain, in the game's order and from one random (sub_14D1ED640)
class TheEndDecorationFeature : public IFeature {
public:
    TheEndDecorationFeature(int64_t seed, const TheEndSimplexNoise &islandNoise);

    ~TheEndDecorationFeature() override = default;

    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

private:
    struct Spike {
        int32_t mX;
        int32_t mZ;
        int32_t mRadius;
        int32_t mHeight;
        bool mGuarded;
    };

    static std::vector<Spike> _createSpikes(uint32_t seed);

    // the surface y of a column, 0 when it holds nothing
    int32_t _surfaceAt(int32_t x, int32_t z) const;

    void _placeSpike(const Spike &spike);

    void _placeIsland(std::mt19937 &random, int32_t x, int32_t y, int32_t z);

    void _placeChorus(std::mt19937 &random, int32_t x, int32_t y, int32_t z);

    void _growChorus(std::mt19937 &random, const Vector3i &origin, const Vector3i &position, int32_t age,
                     std::vector<Vector3i> &plants);

    bool _isClearOfChorus(int32_t x, int32_t y, int32_t z, int32_t exceptX, int32_t exceptZ) const;

    void _placeGateway(int32_t x, int32_t y, int32_t z);

    uint32_t mSeed;
    const TheEndSimplexNoise *mIslandNoise;
    std::vector<Spike> mSpikes;
};
