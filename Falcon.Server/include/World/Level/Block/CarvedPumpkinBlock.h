#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class CarvedPumpkinBlock final : public Block {
public:
    explicit CarvedPumpkinBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    bool isWornOnHead() const override {
        return true;
    }

    void onPlacedInLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                         const BlockState &state) const override;
};
