#pragma once

#include "World/Level/LevelGen/Feature/Ore/OreFeature.h"

class GoldOreMesaFeature : public OreFeature {
public:
    const BlockState &getState(const BlockState &original) const override;

    int32_t getClusterCount() const override;

    int32_t getClusterSize() const override;

    int32_t getMinHeight() const override;

    int32_t getMaxHeight() const override;

    const char *name() const override;
};
