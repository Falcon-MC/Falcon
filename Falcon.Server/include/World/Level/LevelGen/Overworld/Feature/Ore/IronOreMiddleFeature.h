#pragma once

#include "World/Level/LevelGen/Overworld/Feature/Ore/IronOreUpperFeature.h"

class IronOreMiddleFeature : public IronOreUpperFeature {
public:
    int32_t getClusterCount() const override;

    int32_t getClusterSize() const override;

    int32_t getMinHeight() const override;

    int32_t getMaxHeight() const override;

    const char *name() const override;
};
