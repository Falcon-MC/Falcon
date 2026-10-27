#pragma once

#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockTypeIds.h"

#include "Core/Math/Vector3i.h"

class ServerNetworkHandler;
class ServerPlayer;

class CraftingTableBlock final : public Block {
public:
    CraftingTableBlock()
          : Block(BlockTypeIds::CRAFTING_TABLE, "minecraft:crafting_table", "Crafting Table") {}

    explicit CraftingTableBlock(const Block &block) : Block(block) {}

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    static bool matches(const BlockState &state) { return matches(state.mName); }

    static bool matches(const std::string &identifier) { return identifier == "minecraft:crafting_table"; }

    int getFuelTime() const { return 300; }
};
