#pragma once

#include "World/Level/LevelGen/Overworld/Feature/Decoration/ScanSurfaceFeature.h"

class MelonFeature : public ScanSurfaceFeature {
public:
    const char *name() const override;

    int32_t getBase() const override;

    int32_t getRandom() const override;

    void place(BlockManager &manager, int32_t x, int32_t y, int32_t z) override;
};
