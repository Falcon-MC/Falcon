#pragma once

#include "World/Level/Block/Components/BlockBehavior.h"

class IceBlockBehavior : public BlockBehavior {
public:
    float getFrictionFactor() const override {
        return 0.98f;
    }
};
