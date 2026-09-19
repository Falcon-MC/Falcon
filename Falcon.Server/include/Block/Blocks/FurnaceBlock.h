#pragma once

#include "Block/Block.h"
#include "Block/BlockTypeIds.h"
#include "Block/Inventory/FurnaceInventory.h"

#include "Core/Math/Vector3i.h"
#include "Protocol/Types/ContainerType.h"

class ServerNetworkHandler;
class ServerPlayer;

class FurnaceBlock final : public Block {
public:
    FurnaceBlock()
          : Block(BlockTypeIds::FURNACE, "minecraft:furnace", "Furnace") {}

    explicit FurnaceBlock(const Block &block) : Block(block) {}

    static bool matches(const BlockState &state);
    static bool matches(const std::string &identifier);
    static FurnaceKind kind(const BlockState &state);
    static ContainerType containerType(const BlockState &state);

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                  const BlockState &state, const ItemStack &usedItem, int blockFace) const override;

    void onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                  const BlockState &state) const override;

    int getFuelTime() const { return 0; }
};
