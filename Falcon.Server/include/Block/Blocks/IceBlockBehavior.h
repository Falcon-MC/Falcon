#pragma once

#include "Block/Components/BlockBehavior.h"

class IceBlockBehavior : public BlockBehavior {
public:
    float getFrictionFactor() const override {
        return 0.98f;
    }
};

class BlueIceBlockBehavior final : public IceBlockBehavior {
public:
    float getFrictionFactor() const override {
        return 0.989f;
    }
};
