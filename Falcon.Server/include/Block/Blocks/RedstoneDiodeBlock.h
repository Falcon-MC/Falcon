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

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;

    bool isSignalSource() const override;

    virtual bool isPowered(const BlockState &state) const = 0;

    virtual BlockState getPoweredState(const BlockState &state) const = 0;

    virtual BlockState getUnpoweredState(const BlockState &state) const = 0;
};

class RedstoneRepeaterBlock final : public RedstoneDiodeBlock {
public:
    explicit RedstoneRepeaterBlock(const Block &block) : RedstoneDiodeBlock(block)
    {
    }

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    static bool matches(const std::string &identifier);

    bool isPowered(const BlockState &state) const override;

    BlockState getPoweredState(const BlockState &state) const override;

    BlockState getUnpoweredState(const BlockState &state) const override;
};

class RedstoneComparatorBlock final : public RedstoneDiodeBlock {
public:
    explicit RedstoneComparatorBlock(const Block &block) : RedstoneDiodeBlock(block)
    {
    }

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    static bool matches(const std::string &identifier);

    bool isPowered(const BlockState &state) const override;

    BlockState getPoweredState(const BlockState &state) const override;

    BlockState getUnpoweredState(const BlockState &state) const override;
};
