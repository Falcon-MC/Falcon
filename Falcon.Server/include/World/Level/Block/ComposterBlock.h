#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class ServerNetworkHandler;

class ComposterBlock final : public Block {
public:
    explicit ComposterBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    int getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const override;

    // a full composter empties for its bone meal; false when it is not full
    static bool takeOutput(ServerNetworkHandler &owner, Level &level, const Vector3i &position);
};
