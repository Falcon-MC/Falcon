#pragma once

#include "World/Level/Block/Components/BlockBehavior.h"

class SoulSpeedBlockBehavior final : public BlockBehavior {
public:
    bool supportsSoulSpeed() const override {
        return true;
    }
};
