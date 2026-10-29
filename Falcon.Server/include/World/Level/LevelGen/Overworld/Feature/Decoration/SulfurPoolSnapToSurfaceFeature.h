#pragma once

#include "World/Level/LevelGen/Overworld/Feature/Decoration/LakeFeature.h"

class SulfurPoolSnapToSurfaceFeature : public LakeFeature {
public:
    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

protected:
    BlockState fluid() const override;

    BlockState barrier() const override;

    bool canPlaceFeature(BlockManager &manager, int32_t x, int32_t y, int32_t z) const override;

private:
    static void _placePotentSulfur(BlockManager &manager, int32_t x, int32_t y, int32_t z);

    static int32_t _scanUpToSurface(BlockManager &manager, int32_t x, int32_t y, int32_t z, int32_t maxY);
};
