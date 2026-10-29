#pragma once

#include "World/Level/LevelGen/Feature/IFeature.h"

class BlockManager;

class GeodeFeature : public IFeature {
public:
    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;

private:
    static bool _isAmethystShell(BlockManager &manager, int32_t x, int32_t y, int32_t z);
};
