#pragma once

#include "World/Level/LevelGen/Feature/IFeature.h"

class IcicleClusterFeature : public IFeature {
public:
    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;
};
