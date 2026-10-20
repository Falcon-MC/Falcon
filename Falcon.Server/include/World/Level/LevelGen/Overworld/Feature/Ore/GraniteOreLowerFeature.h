#pragma once

#include "World/Level/LevelGen/Overworld/Feature/Ore/StoneOnlyOreFeature.h"

class GraniteOreLowerFeature : public StoneOnlyOreFeature {
public:
    const BlockState &getState(const BlockState &original) const override;

    int32_t getClusterCount() const override;

    int32_t getClusterSize() const override;

    int32_t getMinHeight() const override;

    int32_t getMaxHeight() const override;

    const char *name() const override;
};
