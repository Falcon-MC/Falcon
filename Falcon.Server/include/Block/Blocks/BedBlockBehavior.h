#pragma once

#include "Block/Components/BlockBehavior.h"

class BedBlockBehavior : public BlockBehavior {
public:
    float getLandingBounce() const override {
        return 0.75f;
    }
};
