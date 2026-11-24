#pragma once

#include "World/Level/LevelGen/Structure/StructureFeature.h"

#include <memory>
#include <vector>

/**
 * Strongholds come two ways: up to three under village starts, found along a spiral from the world seed, and one
 * in a quarter of the 200 chunk cells of a fixed grid. Spacing and separation describe that grid.
 */
class StrongholdFeature : public StructureFeature {
public:
    static const int32_t CELL_SIZE = 200;

    StrongholdFeature();

    // the village starts the spiral settled on, in the order it found them
    static std::shared_ptr<const std::vector<StructureChunkPos>> villageStrongholds(const ChunkGenerator &generator);

    // the grid start of the cell holding the chunk; false when the cell draws none
    static bool gridStronghold(int64_t seed, int32_t centerX, int32_t centerZ, StructureChunkPos &out);

    static int32_t cellOf(int32_t chunk);

    StructureChunkPos getCandidate(int64_t seed, int32_t regionX, int32_t regionZ) const override;

    bool isStartChunk(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override;

    bool findNearest(const ChunkGenerator &generator, int32_t blockX, int32_t blockZ,
                     StructureChunkPos &out) const override;

    int32_t getBlockOffset() const override {
        return 2;
    }

protected:
    bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override;
};
