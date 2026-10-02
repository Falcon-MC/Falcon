#pragma once

#include "Block/Block.h"

#include <string>

class MagmaBlock final : public Block {
public:
    explicit MagmaBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    PathHazard getPathHazard() const override {
        return PathHazard::Damaging;
    }

    void onStepOn(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                  const BlockState &state) const override;
};
