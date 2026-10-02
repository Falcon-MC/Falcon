#pragma once

#include "Block/Blocks/IceBlockBehavior.h"

class BlueIceBlockBehavior final : public IceBlockBehavior {
public:
    float getFrictionFactor() const override {
        return 0.989f;
    }
};
