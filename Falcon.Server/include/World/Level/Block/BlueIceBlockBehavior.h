#pragma once

#include "World/Level/Block/IceBlockBehavior.h"

class BlueIceBlockBehavior final : public IceBlockBehavior {
public:
    float getFrictionFactor() const override {
        return 0.989f;
    }
};
