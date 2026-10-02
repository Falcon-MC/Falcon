#pragma once

#include "Level/Generator/Overworld/Feature/Ore/LapisOreBuriedFeature.h"

class LapisOreFeature : public LapisOreBuriedFeature {
public:
    int32_t getClusterCount() const override;

    int32_t getMinHeight() const override;

    int32_t getMaxHeight() const override;

    ConcentrationType getConcentration() const override;

    bool isHostedOre() const override {
        return true;
    }

    const char *name() const override;
};
