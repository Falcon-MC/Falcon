#pragma once

#include "World/Level/LevelGen/Feature/IFeature.h"

class BlockManager;
class LevelChunk;

class UnderwaterMagmaFeature : public IFeature {
public:
    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

private:
    static int32_t _findFloorY(LevelChunk &chunk, int32_t x, int32_t originY, int32_t z);

    static bool _isConnectedToSurfaceWater(LevelChunk &chunk, int32_t x, int32_t originY, int32_t z);

    void _placeMagmaBlob(LevelChunk &chunk, BlockManager &manager, int32_t floorX, int32_t floorY, int32_t floorZ);

    static bool _canPlaceMagma(LevelChunk &chunk, int32_t x, int32_t y, int32_t z);
};
