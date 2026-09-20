#pragma once

#include "Block/Block.h"
#include "Core/Math/Vector3i.h"

#include <string>

class ServerNetworkHandler;
class ServerPlayer;

class LeverBlock final : public Block {
public:
    explicit LeverBlock(const Block &block) : Block(block)
    {
    }

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    static bool matches(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;
};
