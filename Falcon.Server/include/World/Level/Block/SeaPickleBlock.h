#pragma once

#include "World/Level/Block/Block.h"

#include <string>

// bone meal on coral grows the cluster and seeds pickles on the coral around it
class SeaPickleBlock final : public Block {
public:
    explicit SeaPickleBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    bool onFertilized(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;
};
