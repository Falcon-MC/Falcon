#pragma once

#include "World/Level/Block/ChestBlock.h"

#include <string>

class TrappedChestBlock final : public ChestBlock {
public:
    explicit TrappedChestBlock(const Block &block) : ChestBlock(block) {}

    static bool matches(const std::string &identifier);

    bool isSignalSource() const override;

    // the game seats pets on the plain chest only
    bool seatsPet(Level &level, const Vector3i &position, const BlockState &state) const override {
        (void) level;
        (void) position;
        (void) state;
        return false;
    }
};
