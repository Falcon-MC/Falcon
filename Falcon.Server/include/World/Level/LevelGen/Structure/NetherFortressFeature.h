#pragma once

#include "World/Level/LevelGen/Structure/StructureFeature.h"

#include <random>

// the nether complexes grid: one candidate per region, a fortress on a third of them, a bastion on the rest
class NetherFortressFeature : public StructureFeature {
public:
    NetherFortressFeature();

    DimensionType getDimension() const override {
        return DimensionType::Nether;
    }

    // the region random as the start test left it: the pieces go on drawing from it
    std::mt19937 createFortressRandom(int64_t seed, int32_t chunkX, int32_t chunkZ) const;

protected:
    bool _isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const override;
};
