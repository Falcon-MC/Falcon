#pragma once

#include "Block/Blocks/FacingMachineBlock.h"

#include <string>

class PistonBlock final : public FacingMachineBlock {
public:
    explicit PistonBlock(const Block &block) : FacingMachineBlock(block)
    {
    }

    static bool matches(const std::string &identifier);

    void onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                  const BlockState &state) const override;
};
