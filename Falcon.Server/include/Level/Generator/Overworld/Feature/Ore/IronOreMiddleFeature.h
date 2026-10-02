#pragma once

#include "Level/Generator/Overworld/Feature/Ore/IronOreUpperFeature.h"

class IronOreMiddleFeature : public IronOreUpperFeature {
public:
    int32_t getClusterCount() const override;

    int32_t getClusterSize() const override;

    int32_t getMinHeight() const override;

    int32_t getMaxHeight() const override;

    bool isHostedOre() const override {
        return true;
    }

    const char *name() const override;
};
