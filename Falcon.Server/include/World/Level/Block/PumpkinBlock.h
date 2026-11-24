#pragma once

#include "World/Level/Block/Block.h"

#include <cstdint>
#include <string>

class PumpkinBlock final : public Block {
public:
    explicit PumpkinBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    // shears turn it into a carved pumpkin facing the clicked side and drop a seed
    void carve(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position, int32_t face) const;

    // the carved face for a clicked face: up and down keep the default, south
    static const char *carvedDirection(int32_t face);
};
