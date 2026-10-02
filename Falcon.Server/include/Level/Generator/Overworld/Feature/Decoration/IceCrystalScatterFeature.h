#pragma once

#include "Level/Generator/Feature/IFeature.h"

class IceCrystalScatterFeature : public IFeature {
public:
    const char *name() const override;

    void apply(ChunkGenerateContext &context) override;
};
