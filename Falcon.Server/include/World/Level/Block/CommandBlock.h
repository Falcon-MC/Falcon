#pragma once

#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockTypeIds.h"

#include "Core/Math/Vector3i.h"

class ServerNetworkHandler;
class ServerPlayer;

class CommandBlock final : public Block {
public:
    CommandBlock()
          : Block(BlockTypeIds::COMMAND_BLOCK, "minecraft:command_block", "Command Block") {}

    explicit CommandBlock(const Block &block) : Block(block) {}

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    static bool matches(const BlockState &state) { return matches(state.mName); }

    static bool matches(const std::string &identifier);
};
