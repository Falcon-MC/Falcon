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
