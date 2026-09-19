#pragma once

#include "Block/Block.h"
#include "Core/Math/Vector3i.h"

#include <string>

class ServerNetworkHandler;
class ServerPlayer;

class RedstoneDiodeBlock : public Block {
public:
    explicit RedstoneDiodeBlock(const Block &block) : Block(block)
    {
    }

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;
};

class RedstoneRepeaterBlock final : public RedstoneDiodeBlock {
public:
    explicit RedstoneRepeaterBlock(const Block &block) : RedstoneDiodeBlock(block)
    {
    }

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    static bool matches(const std::string &identifier);
};

class RedstoneComparatorBlock final : public RedstoneDiodeBlock {
public:
    explicit RedstoneComparatorBlock(const Block &block) : RedstoneDiodeBlock(block)
    {
    }

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    static bool matches(const std::string &identifier);
};
