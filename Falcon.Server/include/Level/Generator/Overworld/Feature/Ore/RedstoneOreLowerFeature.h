#pragma once

#include "Level/Generator/Overworld/Feature/Ore/RedstoneOreFeature.h"

class RedstoneOreLowerFeature : public RedstoneOreFeature {
public:
    int32_t getClusterCount() const override;

    int32_t getMaxHeight() const override;

    ConcentrationType getConcentration() const override;

    bool isHostedOre() const override {
        return true;
    }

    const char *name() const override;
};
