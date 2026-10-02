#pragma once

#include "Level/Generator/Overworld/Feature/Ore/StoneOnlyOreFeature.h"

class GravelOreFeature : public StoneOnlyOreFeature {
public:
    const BlockState &getState(const BlockState &original) const override;

    int32_t getClusterCount() const override;

    int32_t getClusterSize() const override;

    int32_t getMinHeight() const override;

    int32_t getMaxHeight() const override;

    bool isIntrusiveDeposit() const override {
        return true;
    }

    const char *name() const override;
};
