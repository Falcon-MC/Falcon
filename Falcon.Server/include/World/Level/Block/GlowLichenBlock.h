#pragma once

#include "World/Level/Block/MultifaceBlock.h"

#include <string>

// the multiface block that bone meal spreads
class GlowLichenBlock final : public MultifaceBlock {
public:
    explicit GlowLichenBlock(const Block &block) : MultifaceBlock(block) {}

    static bool matches(const std::string &identifier);

    bool onFertilized(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;
};
