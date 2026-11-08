#pragma once

#include "World/Item/Item.h"
#include "World/Level/LevelGen/End/Feature/EndSpikes.h"
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

    /** An end gateway with its bedrock frame, as the islands and the dragon fight both place it. */
    static void placeGateway(int32_t x, int32_t y, int32_t z, const EndSpikes::BlockSink &sink);

    /** The end stone island of the outer islands and of a gateway exit without land (sub_14A2BC410). */
    static void placeIsland(std::mt19937 &random, int32_t x, int32_t y, int32_t z, const EndSpikes::BlockSink &sink);

private:
    // the surface y of a column, 0 when it holds nothing
    int32_t _surfaceAt(int32_t x, int32_t z) const;

    void _placeSpike(ChunkGenerateContext &context, const EndSpike &spike);

    void _placeIsland(std::mt19937 &random, int32_t x, int32_t y, int32_t z);

    void _placeChorus(std::mt19937 &random, int32_t x, int32_t y, int32_t z);

    void _growChorus(std::mt19937 &random, const Vector3i &origin, const Vector3i &position, int32_t age,
                     std::vector<Vector3i> &plants);

    bool _isClearOfChorus(int32_t x, int32_t y, int32_t z, int32_t exceptX, int32_t exceptZ) const;

    uint32_t mSeed;
    const TheEndSimplexNoise *mIslandNoise;
    std::vector<EndSpike> mSpikes;
    std::mt19937 mSpikeRandom;
};
