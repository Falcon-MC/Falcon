#pragma once

#include "Level/Generator/Feature/Ore/OreFeature.h"

class DirtOreFeature : public OreFeature {
public:
    const BlockState &getState(const BlockState &original) const override;

    int32_t getClusterCount() const override;

    int32_t getClusterSize() const override;

    int32_t getMinHeight() const override;

    int32_t getMaxHeight() const override;

    bool isIntrusiveDeposit() const override {
        return true;
    }

    const char *getExcludingBiomeTag() const override {
        return "no_dirt";
    }

    const char *name() const override;
};
