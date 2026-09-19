#pragma once

#include "Block/Block.h"
#include "Protocol/Types/ContainerType.h"

#include <memory>
#include <string>

class BlockActor;
class ItemStack;
class Level;
class ServerNetworkHandler;

enum class ContainerBlockKind {
    Barrel,
    ShulkerBox,
    EnderChest,
    Hopper,
    Dispenser,
    Dropper,
    BrewingStand,
    Beacon,
    EnchantTable,
    Crafter,
    Campfire,
    Lectern,
    ChiseledBookshelf,
    DecoratedPot,
    Jukebox,
    Shelf
};

struct ContainerBlockDefinition {
    ContainerBlockKind mKind;
    const char *mBlockActorId;
    ContainerType mContainerType;
    bool mOpensWindow;
    bool mDropsContentsOnBreak;
};

class ContainerBlock : public Block {
public:
    explicit ContainerBlock(const Block &base);

    static bool matches(const std::string &identifier);

    static const ContainerBlockDefinition *findDefinition(const std::string &identifier);

    static std::unique_ptr<BlockActor> createBlockActor(ContainerBlockKind kind);

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    BlockState applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const override;

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                  const BlockState &state, const ItemStack &usedItem, int blockFace) const override;

    void onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                  const BlockState &state) const override;

    void writeDropContents(const Vector3i &position, ItemStack &item) const override;
};
