#pragma once

#include "Block/Block.h"

#include <memory>
#include <string>

class BlockActor;
class ItemStack;
class Level;
class ServerNetworkHandler;

class ItemFrameBlock : public Block {
public:
    explicit ItemFrameBlock(const Block &base);

    static bool matches(const std::string &identifier);

    static bool isGlow(const std::string &identifier);

    static std::unique_ptr<BlockActor> createBlockActor(const std::string &identifier);

    bool canPlaceAt(Level &level, const Vector3i &position, int blockFace) const override;

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;

    bool onPunch(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                 const BlockState &state) const override;

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                  const BlockState &state, const ItemStack &usedItem, int blockFace) const override;

    void onBroken(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state) const override;
};
