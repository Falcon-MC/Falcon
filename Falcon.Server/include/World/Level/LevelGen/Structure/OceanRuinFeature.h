#pragma once

#include "World/Level/LevelGen/Structure/StructureFeature.h"

class OceanRuinFeature : public StructureFeature {
public:
    // what the biome gives the ruins it holds (Biome +68): set only on the ocean biomes
    struct Config {
        bool mWarm = false;
        float mLargeChance = 0.0f;
        float mClusterChance = 0.0f;
    };

    OceanRuinFeature();

    // false for a biome without ruins
    static bool getConfig(int32_t biomeId, Config &out);

protected:
    bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override;
};
