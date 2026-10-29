#pragma once

#include "World/Level/LevelGen/Feature/IFeature.h"

class IceCavesSurfaceFeature : public IFeature {
public:
    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;
};
