#pragma once

#include "World/Level/LevelGen/Overworld/Feature/Ore/LapisOreBuriedFeature.h"

class LapisOreFeature : public LapisOreBuriedFeature {
public:
    int32_t getClusterCount() const override;

    int32_t getMinHeight() const override;

    int32_t getMaxHeight() const override;

    ConcentrationType getConcentration() const override;

    const char *name() const override;
};
