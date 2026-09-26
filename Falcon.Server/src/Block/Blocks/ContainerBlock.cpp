#include "Block/Blocks/ContainerBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(ContainerBlock, 50);

#include "Actor/ServerPlayer.h"
#include "Block/Actor/ContainerBlockActor.h"
#include "Block/Actor/EnderChestBlockActor.h"
#include "Block/Actor/HopperBlockActor.h"
#include "Block/BlockActorStore.h"
#include "Block/Components/PlacementOrientation.h"
#include "Inventory/Container/BlockContainerManagerModel.h"
#include "Inventory/InventoryManager.h"
#include "Inventory/PlayerInventory.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"

#include <utility>

namespace {
    const char *TAG_ITEMS = "Items";

    const ContainerBlockDefinition DEFINITIONS[] = {
            {ContainerBlockKind::Barrel,            BarrelBlockActor::BLOCK_ACTOR_ID,            ContainerType::Container,    true,  true},
            {ContainerBlockKind::ShulkerBox,        ShulkerBoxBlockActor::BLOCK_ACTOR_ID,        ContainerType::Container,    true,  false},
            {ContainerBlockKind::EnderChest,        EnderChestBlockActor::BLOCK_ACTOR_ID,        ContainerType::Container,    true,  false},
            {ContainerBlockKind::Hopper,            HopperBlockActor::BLOCK_ACTOR_ID,            ContainerType::Hopper,       true,  true},
            {ContainerBlockKind::Dispenser,         DispenserBlockActor::BLOCK_ACTOR_ID,         ContainerType::Dispenser,    true,  true},
            {ContainerBlockKind::Dropper,           DropperBlockActor::BLOCK_ACTOR_ID,           ContainerType::Dropper,      true,  true},
            {ContainerBlockKind::BrewingStand,      BrewingStandBlockActor::BLOCK_ACTOR_ID,      ContainerType::BrewingStand, true,  true},
            {ContainerBlockKind::Beacon,            BeaconBlockActor::BLOCK_ACTOR_ID,            ContainerType::Beacon,       true,  true},
            {ContainerBlockKind::EnchantTable,      EnchantTableBlockActor::BLOCK_ACTOR_ID,      ContainerType::Enchantment,  true,  true},
            {ContainerBlockKind::Crafter,           CrafterBlockActor::BLOCK_ACTOR_ID,           ContainerType::Crafter,      true,  true},
            {ContainerBlockKind::Campfire,          CampfireBlockActor::BLOCK_ACTOR_ID,          ContainerType::None,         false, true},
            {ContainerBlockKind::Lectern,           LecternBlockActor::BLOCK_ACTOR_ID,           ContainerType::None,         false, true},
            {ContainerBlockKind::ChiseledBookshelf, ChiseledBookshelfBlockActor::BLOCK_ACTOR_ID, ContainerType::None,         false, true},
            {ContainerBlockKind::DecoratedPot,      DecoratedPotBlockActor::BLOCK_ACTOR_ID,      ContainerType::None,         false, true},
            {ContainerBlockKind::Jukebox,           JukeboxBlockActor::BLOCK_ACTOR_ID,           ContainerType::None,         false, true},
            {ContainerBlockKind::Shelf,             ShelfBlockActor::BLOCK_ACTOR_ID,             ContainerType::None,         false, true}
    };

    const ContainerBlockDefinition &definitionOf(ContainerBlockKind kind) {
        for (const ContainerBlockDefinition &definition: DEFINITIONS) {
            if (definition.mKind == kind)
                return definition;
        }

        return DEFINITIONS[0];
    }

    bool endsWith(const std::string &value, const std::string &suffix) {
        if (value.size() < suffix.size())
            return false;

        return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    bool kindOf(const std::string &identifier, ContainerBlockKind &outKind) {
        if (identifier == "minecraft:barrel") {
            outKind = ContainerBlockKind::Barrel;
            return true;
        }
        if (endsWith(identifier, "_shulker_box")) {
            outKind = ContainerBlockKind::ShulkerBox;
            return true;
        }
        if (identifier == "minecraft:ender_chest") {
            outKind = ContainerBlockKind::EnderChest;
            return true;
        }
        if (identifier == "minecraft:hopper") {
            outKind = ContainerBlockKind::Hopper;
            return true;
        }
        if (identifier == "minecraft:dispenser") {
            outKind = ContainerBlockKind::Dispenser;
            return true;
        }
        if (identifier == "minecraft:dropper") {
            outKind = ContainerBlockKind::Dropper;
            return true;
        }
        if (identifier == "minecraft:brewing_stand") {
            outKind = ContainerBlockKind::BrewingStand;
            return true;
        }
        if (identifier == "minecraft:beacon") {
            outKind = ContainerBlockKind::Beacon;
            return true;
        }
        if (identifier == "minecraft:enchanting_table") {
            outKind = ContainerBlockKind::EnchantTable;
            return true;
        }
        if (identifier == "minecraft:crafter") {
            outKind = ContainerBlockKind::Crafter;
            return true;
        }
        if (identifier == "minecraft:campfire" || identifier == "minecraft:soul_campfire") {
            outKind = ContainerBlockKind::Campfire;
            return true;
        }
        if (identifier == "minecraft:lectern") {
            outKind = ContainerBlockKind::Lectern;
            return true;
        }
        if (identifier == "minecraft:chiseled_bookshelf") {
            outKind = ContainerBlockKind::ChiseledBookshelf;
            return true;
        }
        if (identifier == "minecraft:decorated_pot") {
            outKind = ContainerBlockKind::DecoratedPot;
            return true;
        }
        if (identifier == "minecraft:jukebox") {
            outKind = ContainerBlockKind::Jukebox;
            return true;
        }
        if (endsWith(identifier, "_shelf")) {
            outKind = ContainerBlockKind::Shelf;
            return true;
        }

        return false;
    }

    bool placeCampfireFood(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) {
        PlayerInventory &inventory = player.getInventory();
        ItemStack held = inventory.getItemInHand();
        if (held.isAir() || held.mCount <= 0)
            return false;

        Level &level = owner.getLevelFor(player);
        CampfireBlockActor &campfire = level.getBlockActors().getOrCreate<CampfireBlockActor>(position);
        if (!campfire.addFood(held))
            return false;

        if (player.getGameType() != (int32_t) GameType::Creative) {
            --held.mCount;
            if (held.mCount <= 0)
                held = ItemStack::air();
            inventory.setItemInHand(std::move(held));
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                                  inventory.getSelectedSlot());
        }

        BlockActionHandler::broadcastBlockActorData(owner, level, campfire);
        return true;
    }
}

ContainerBlock::ContainerBlock(const Block &base) : Block(base) {
}

bool ContainerBlock::matches(const std::string &identifier) {
    ContainerBlockKind kind = ContainerBlockKind::Barrel;
    return kindOf(identifier, kind);
}

const ContainerBlockDefinition *ContainerBlock::findDefinition(const std::string &identifier) {
    ContainerBlockKind kind = ContainerBlockKind::Barrel;
    if (!kindOf(identifier, kind))
        return nullptr;

    return &definitionOf(kind);
}

std::unique_ptr<BlockActor> ContainerBlock::createBlockActor(ContainerBlockKind kind) {
    switch (kind) {
        case ContainerBlockKind::Barrel:
            return std::unique_ptr<BlockActor>(new BarrelBlockActor());
        case ContainerBlockKind::ShulkerBox:
            return std::unique_ptr<BlockActor>(new ShulkerBoxBlockActor());
        case ContainerBlockKind::EnderChest:
            return std::unique_ptr<BlockActor>(new EnderChestBlockActor());
        case ContainerBlockKind::Hopper:
            return std::unique_ptr<BlockActor>(new HopperBlockActor());
        case ContainerBlockKind::Dispenser:
            return std::unique_ptr<BlockActor>(new DispenserBlockActor());
        case ContainerBlockKind::Dropper:
            return std::unique_ptr<BlockActor>(new DropperBlockActor());
        case ContainerBlockKind::BrewingStand:
            return std::unique_ptr<BlockActor>(new BrewingStandBlockActor());
        case ContainerBlockKind::Beacon:
            return std::unique_ptr<BlockActor>(new BeaconBlockActor());
        case ContainerBlockKind::EnchantTable:
            return std::unique_ptr<BlockActor>(new EnchantTableBlockActor());
        case ContainerBlockKind::Crafter:
            return std::unique_ptr<BlockActor>(new CrafterBlockActor());
        case ContainerBlockKind::Campfire:
            return std::unique_ptr<BlockActor>(new CampfireBlockActor());
        case ContainerBlockKind::Lectern:
            return std::unique_ptr<BlockActor>(new LecternBlockActor());
        case ContainerBlockKind::ChiseledBookshelf:
            return std::unique_ptr<BlockActor>(new ChiseledBookshelfBlockActor());
        case ContainerBlockKind::DecoratedPot:
            return std::unique_ptr<BlockActor>(new DecoratedPotBlockActor());
        case ContainerBlockKind::Jukebox:
            return std::unique_ptr<BlockActor>(new JukeboxBlockActor());
        case ContainerBlockKind::Shelf:
            return std::unique_ptr<BlockActor>(new ShelfBlockActor());
    }

    return nullptr;
}

bool ContainerBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                const BlockState &state) const {
    const ContainerBlockDefinition *definition = findDefinition(state.mName);
    if (definition == nullptr)
        return false;

    if (definition->mKind == ContainerBlockKind::Campfire)
        return placeCampfireFood(owner, player, position);

    if (!definition->mOpensWindow)
        return false;

    BlockActorStore &blockActors = owner.getLevelFor(player).getBlockActors();
    if (blockActors.find(position) == nullptr) {
        std::unique_ptr<BlockActor> created = createBlockActor(definition->mKind);
        if (created == nullptr)
            return false;

        created->setPosition(position);
        blockActors.insert(std::move(created));
    }

    BlockContainerManagerModel model(definition->mContainerType);
    model.open(owner, player, position);
    return true;
}

void ContainerBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                              const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    const ContainerBlockDefinition *definition = findDefinition(state.mName);
    if (definition == nullptr)
        return;

    BlockActorStore &blockActors = owner.getLevelFor(player).getBlockActors();
    blockActors.remove(position);

    std::unique_ptr<BlockActor> created = createBlockActor(definition->mKind);
    if (created == nullptr)
        return;

    created->setPosition(position);

    if (definition->mKind == ContainerBlockKind::ShulkerBox) {
        ShulkerBoxBlockActor *shulker = dynamic_cast<ShulkerBoxBlockActor *>(created.get());
        if (shulker != nullptr) {
            if (blockFace >= 0 && blockFace <= 5)
                shulker->setFacing((int8_t) blockFace);

            const Tag *items = usedItem.mTag.getType() == Tag::Type::Compound
                               ? usedItem.mTag.get(TAG_ITEMS)
                               : nullptr;
            if (items != nullptr)
                shulker->getInventory().loadItems(*items, owner.getCodecContext());
        }
    }

    blockActors.insert(std::move(created));
}

void ContainerBlock::onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state) const {
    const ContainerBlockDefinition *definition = findDefinition(state.mName);
    if (definition == nullptr)
        return;

    if (definition->mDropsContentsOnBreak) {
        ContainerBlockActor *actor = level.getBlockActors().find<ContainerBlockActor>(position);
        if (actor != nullptr) {
            const Vector3f dropPosition((float) position.x + 0.5f, (float) position.y + 0.5f,
                                        (float) position.z + 0.5f);
            SimpleContainerInventory &inventory = actor->getInventory();

            for (int slot = 0; slot < inventory.getContainerSize(); ++slot) {
                const ItemStack &item = inventory.getContainerItem(slot);
                if (!item.isAir() && item.mCount > 0) {
                    ItemActorHandler::dropItem(owner, level, dropPosition, item,
                                               ItemActorHandler::randomDropMotion(),
                                               ItemActorHandler::DROP_PICKUP_DELAY);
                }
            }

            inventory.clear();
        }
    }

    level.getBlockActors().remove(position);
}

void ContainerBlock::writeDropContents(Level &level, const Vector3i &position, ItemStack &item) const {
    const ContainerBlockDefinition *definition = findDefinition(getIdentifier());
    if (definition == nullptr || definition->mKind != ContainerBlockKind::ShulkerBox)
        return;

    ContainerBlockActor *actor = level.getBlockActors().find<ContainerBlockActor>(position);
    if (actor == nullptr || actor->getInventory().isEmpty())
        return;

    if (item.mTag.getType() != Tag::Type::Compound)
        item.mTag = Tag::ofCompound();

    item.mTag.put(TAG_ITEMS, actor->getInventory().saveItems());
}

BlockState ContainerBlock::applyPlacementOrientation(const BlockState &state,
                                                     const BlockPlacementContext &context) const {
    using namespace PlacementOrientation;

    BlockState result = Block::applyPlacementOrientation(state, context);
    Tag states = result.mStates;

    const ContainerBlockDefinition *definition = findDefinition(result.mName);
    if (definition == nullptr)
        return BlockState(result.mName, states);

    const bool hasIntFacing = states.contains("facing_direction");
    const bool hasStringFacing = states.contains("minecraft:facing_direction");

    switch (definition->mKind) {
        case ContainerBlockKind::Hopper: {
            const int facing = context.mFace == FACE_DOWN
                               ? FACE_DOWN
                               : (context.mFace == FACE_UP ? FACE_DOWN : context.mFace ^ 1);
            if (hasIntFacing)
                states.putInt("facing_direction", facing);
            if (hasStringFacing)
                states.putString("minecraft:facing_direction", faceName(facing));
            break;
        }
        case ContainerBlockKind::Dispenser:
        case ContainerBlockKind::Dropper: {
            if (hasIntFacing)
                states.putInt("facing_direction", context.mPistonFacing);
            if (hasStringFacing)
                states.putString("minecraft:facing_direction", faceName(context.mPistonFacing));
            break;
        }
        case ContainerBlockKind::ShulkerBox: {
            if (hasIntFacing)
                states.putInt("facing_direction", context.mFace);
            if (hasStringFacing)
                states.putString("minecraft:facing_direction", faceName(context.mFace));
            break;
        }
        case ContainerBlockKind::Shelf:
        case ContainerBlockKind::Campfire: {
            if (states.contains("minecraft:cardinal_direction"))
                states.putString("minecraft:cardinal_direction", cardinalName(context.mPlayerFacing));
            break;
        }
        default:
            break;
    }

    return BlockState(result.mName, states);
}
