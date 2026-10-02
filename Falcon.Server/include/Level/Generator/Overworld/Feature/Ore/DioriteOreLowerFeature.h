#pragma once

#include "Level/Generator/Overworld/Feature/Ore/GraniteOreLowerFeature.h"

class DioriteOreLowerFeature : public GraniteOreLowerFeature {
public:
    const BlockState &getState(const BlockState &original) const override;

    bool isIntrusiveDeposit() const override {
        return true;
    }

    const char *name() const override;
};
