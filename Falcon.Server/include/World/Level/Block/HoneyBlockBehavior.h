#pragma once

#include "World/Level/Block/Components/BlockBehavior.h"

class HoneyBlockBehavior : public BlockBehavior {
public:
    bool slidesAlongSides() const override {
        return true;
    }

    bool slowsWalking() const override {
        return true;
    }
};
