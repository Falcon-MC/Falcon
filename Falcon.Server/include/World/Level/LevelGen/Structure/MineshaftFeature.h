#pragma once

#include "World/Level/LevelGen/Structure/StructureFeature.h"

class MineshaftFeature final : public StructureFeature {
public:
    MineshaftFeature();

    bool isStartChunk(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override;
    bool findNearest(const ChunkGenerator &generator, int32_t blockX, int32_t blockZ,
                     StructureChunkPos &out) const override;
    int32_t getBlockOffset() const override {
        return 0;
    }

    // the start builder continues drawing from the random left by the eligibility check
    static bool rollStart(int64_t seed, int32_t chunkX, int32_t chunkZ, std::mt19937 &random);

protected:
    bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override;
};
