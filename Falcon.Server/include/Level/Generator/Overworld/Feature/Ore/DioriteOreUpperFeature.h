#pragma once

#include "Level/Generator/Overworld/Feature/Ore/GraniteOreUpperFeature.h"

class DioriteOreUpperFeature : public GraniteOreUpperFeature {
public:
    const BlockState &getState(const BlockState &original) const override;

    bool isIntrusiveDeposit() const override {
        return true;
    }

    const char *name() const override;
};
