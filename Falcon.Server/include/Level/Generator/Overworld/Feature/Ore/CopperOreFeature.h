#pragma once

#include "Level/Generator/Feature/Ore/OreFeature.h"

class CopperOreFeature : public OreFeature {
public:
    const BlockState &getState(const BlockState &original) const override;

    int32_t getClusterCount() const override;

    int32_t getClusterSize() const override;

    int32_t getMinHeight() const override;

    int32_t getMaxHeight() const override;

    ConcentrationType getConcentration() const override;

    bool isHostedOre() const override {
        return true;
    }

    const char *name() const override;
};
