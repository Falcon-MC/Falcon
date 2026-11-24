#include "World/Level/Block/ContainerBlock.h"

#include "Core/Math/VectorMath.h"
#include "World/Item/ItemProperties.h"
#include "World/Level/Vibration/GameEvent.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/LiquidProperties.h"

FALCON_REGISTER_BLOCK(ContainerBlock, 50);

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BeaconBlockActor.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/Actor/EnderChestBlockActor.h"
#include "World/Level/Block/Actor/HopperBlockActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Containers/Managers/Models/BlockContainerManagerModel.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/ItemDefinition.h"
#include "Protocol/Types/StartGameTypes.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"

#include <array>
#include <utility>

namespace {
    const char *TAG_ITEMS = "Items";

    const ContainerBlockDefinition DEFINITIONS[] = {
            {ContainerBlockKind::Barrel, BarrelBlockActor::BLOCK_ACTOR_ID, ContainerType::Container, true, true},
            {ContainerBlockKind::ShulkerBox, ShulkerBoxBlockActor::BLOCK_ACTOR_ID, ContainerType::Container, true,
             false},
            {ContainerBlockKind::EnderChest, EnderChestBlockActor::BLOCK_ACTOR_ID, ContainerType::Container, true,
             false},
            {ContainerBlockKind::Hopper, HopperBlockActor::BLOCK_ACTOR_ID, ContainerType::Hopper, true, true},
            {ContainerBlockKind::Dispenser, DispenserBlockActor::BLOCK_ACTOR_ID, ContainerType::Dispenser, true, true},
            {ContainerBlockKind::Dropper, DropperBlockActor::BLOCK_ACTOR_ID, ContainerType::Dropper, true, true},
            {ContainerBlockKind::BrewingStand, BrewingStandBlockActor::BLOCK_ACTOR_ID, ContainerType::BrewingStand,
             true, true},
            {ContainerBlockKind::Beacon, BeaconBlockActor::BLOCK_ACTOR_ID, ContainerType::Beacon, true, false},
            {ContainerBlockKind::EnchantTable, EnchantTableBlockActor::BLOCK_ACTOR_ID, ContainerType::Enchantment, true,
             true},
            {ContainerBlockKind::Crafter, CrafterBlockActor::BLOCK_ACTOR_ID, ContainerType::Crafter, true, true},
            {ContainerBlockKind::Campfire, CampfireBlockActor::BLOCK_ACTOR_ID, ContainerType::None, false, true},
            {ContainerBlockKind::Lectern, LecternBlockActor::BLOCK_ACTOR_ID, ContainerType::None, false, true},
            {ContainerBlockKind::ChiseledBookshelf, ChiseledBookshelfBlockActor::BLOCK_ACTOR_ID, ContainerType::None,
             false, true},
            {ContainerBlockKind::DecoratedPot, DecoratedPotBlockActor::BLOCK_ACTOR_ID, ContainerType::None, false,
             true},
            {ContainerBlockKind::Shelf, ShelfBlockActor::BLOCK_ACTOR_ID, ContainerType::None, false, true}};

    const ContainerBlockDefinition &definitionOf(ContainerBlockKind kind) {
        for (const ContainerBlockDefinition &definition: DEFINITIONS) {
            if (definition.mKind == kind)
                return definition;
        }

        return DEFINITIONS[0];
    }

    using BlockIdentifier::endsWith;

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

        if (!player.isCreative()) {
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

namespace {
    // the face the shelf shows its items on, from minecraft:cardinal_direction
    int shelfFront(const BlockState &state) {
        const std::string direction = state.mStates.getString("minecraft:cardinal_direction", "south");
        return direction == "west" ? 4 : direction == "north" ? 2 : direction == "east" ? 5 : 3;
    }

    // the game cuts the front in thirds across, at 0.3125 and 0.6875, and only reads the band from 0.25 to 0.6875
    // up; -1 off them
    int shelfSlotAt(int face, const Vector3f &click) {
        float across = 0.0f;
        switch (face) {
            case 2:
                across = 1.0f - click.x;
                break;
            case 3:
                across = click.x;
                break;
            case 4:
                across = click.z;
                break;
            case 5:
                across = 1.0f - click.z;
                break;
            default:
                return -1;
        }
        if (click.y < 0.25f || click.y > 0.6875f)
            return -1;
        if (across >= 0.0f && across <= 0.3125f)
            return 0;
        if (across >= 0.3125f && across <= 0.6875f)
            return 1;
        if (across >= 0.6875f && across <= 1.0f)
            return 2;
        return -1;
    }

    // powered shelves of one facing join in a row of up to three: powered_shelf_type 1 at the end on the
    // (facing - 1) side, 3 at the end on the (facing + 1) side, 2 between them, 0 alone
    const int ROW_ALONE = 0;
    const int ROW_FIRST = 1;
    const int ROW_MIDDLE = 2;
    const int ROW_LAST = 3;

    // minecraft:cardinal_direction as the game numbers it: south, west, north, east
    int shelfFacing(const BlockState &state) {
        const std::string direction = state.mStates.getString("minecraft:cardinal_direction", "south");
        return direction == "west" ? 1 : direction == "north" ? 2 : direction == "east" ? 3 : 0;
    }

    Vector3i stepped(const Vector3i &position, int direction, int count = 1) {
        static const int32_t X[4] = {0, -1, 0, 1};
        static const int32_t Z[4] = {1, 0, -1, 0};
        return Vector3i(position.x + X[direction & 3] * count, position.y, position.z + Z[direction & 3] * count);
    }

    const BlockState *shelfState(Level &level, const Vector3i &position) {
        const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z);
        const ContainerBlockDefinition *definition =
                state == nullptr ? nullptr : ContainerBlock::findDefinition(state->mName);
        return definition != nullptr && definition->mKind == ContainerBlockKind::Shelf ? state : nullptr;
    }

    bool isPowered(const BlockState &state) {
        return state.mStates.getByte("powered_bit") != 0;
    }

    // a powered shelf at that place, of that row type
    bool isRowType(Level &level, const Vector3i &position, int type) {
        const BlockState *state = shelfState(level, position);
        return state != nullptr && isPowered(*state) && state->mStates.getInt("powered_shelf_type") == type;
    }

    // a powered shelf at that place, facing the way the block at the first place faces
    bool joins(Level &level, const Vector3i &position, const Vector3i &other) {
        const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z);
        const BlockState *neighbour = shelfState(level, other);
        return state != nullptr && neighbour != nullptr && isPowered(*neighbour)
               && shelfFacing(*state) == shelfFacing(*neighbour);
    }

    // written without updates: a neighbour told of a half changed row would undo it; the caller notifies once
    // every shelf of the row has its type
    void setRow(Level &level, std::vector<Vector3i> &changed, const Vector3i &position, bool powered, int type) {
        const BlockState *state = shelfState(level, position);
        if (state == nullptr)
            return;
        Tag states = state->mStates;
        states.putByte("powered_bit", powered ? 1 : 0);
        states.putInt("powered_shelf_type", type);
        level.setBlock(position, BlockState(state->mName, states), false);
        changed.push_back(position);
    }

    // a row that lost a shelf without hearing of it: the shelf takes the type its neighbours leave it
    void checkRow(Level &level, std::vector<Vector3i> &changed, const Vector3i &position, const BlockState &state) {
        if (!isPowered(state))
            return;
        const int facing = shelfFacing(state);
        const Vector3i last = stepped(position, facing + 1);
        const Vector3i first = stepped(position, facing - 1);
        // a neighbour in a chunk not loaded is unknown, not gone: the row is left as it is
        if (level.peekBlockPtr(last.x, last.y, last.z) == nullptr
            || level.peekBlockPtr(first.x, first.y, first.z) == nullptr)
            return;
        const int type = state.mStates.getInt("powered_shelf_type");
        if (type == ROW_FIRST || type == ROW_LAST) {
            const Vector3i &other = type == ROW_FIRST ? last : first;
            const int end = type == ROW_FIRST ? ROW_LAST : ROW_FIRST;
            if (isRowType(level, other, end) || isRowType(level, other, ROW_MIDDLE))
                return;
            if (!joins(level, position, other))
                setRow(level, changed, position, true, ROW_ALONE);
            return;
        }
        if (type != ROW_MIDDLE)
            return;
        bool lastJoined = isRowType(level, last, ROW_LAST);
        if (lastJoined && isRowType(level, first, ROW_FIRST))
            return;
        if (!lastJoined) {
            lastJoined = joins(level, position, last);
            if (isRowType(level, first, ROW_FIRST)) {
                if (!lastJoined)
                    setRow(level, changed, position, true, ROW_LAST);
                return;
            }
        }
        const bool firstJoined = joins(level, position, first);
        if (firstJoined && lastJoined)
            return;
        setRow(level, changed, position, true, firstJoined ? ROW_LAST : ROW_FIRST);
    }

    void powerOn(Level &level, std::vector<Vector3i> &changed, const Vector3i &position, int facing) {
        const Vector3i last = stepped(position, facing + 1);
        const Vector3i farLast = stepped(position, facing + 1, 2);
        const Vector3i first = stepped(position, facing - 1);
        const Vector3i farFirst = stepped(position, facing - 1, 2);
        if (joins(level, position, last) && joins(level, position, farLast) && isRowType(level, last, ROW_FIRST)
            && isRowType(level, farLast, ROW_LAST)) {
            setRow(level, changed, position, true, ROW_FIRST);
            setRow(level, changed, last, true, ROW_MIDDLE);
            return;
        }
        const bool lastAlone = joins(level, position, last) && isRowType(level, last, ROW_ALONE);
        const bool firstAlone = joins(level, position, first) && isRowType(level, first, ROW_ALONE);
        if (lastAlone && firstAlone) {
            setRow(level, changed, first, true, ROW_FIRST);
            setRow(level, changed, position, true, ROW_MIDDLE);
            setRow(level, changed, last, true, ROW_LAST);
            return;
        }
        if (lastAlone) {
            setRow(level, changed, position, true, ROW_FIRST);
            setRow(level, changed, last, true, ROW_LAST);
            return;
        }
        if (joins(level, position, first) && joins(level, position, farFirst) && isRowType(level, first, ROW_LAST)
            && isRowType(level, farFirst, ROW_FIRST)) {
            setRow(level, changed, first, true, ROW_MIDDLE);
            setRow(level, changed, position, true, ROW_LAST);
            return;
        }
        if (firstAlone) {
            setRow(level, changed, first, true, ROW_FIRST);
            setRow(level, changed, position, true, ROW_LAST);
            return;
        }
        setRow(level, changed, position, true, ROW_ALONE);
    }

    void powerOff(Level &level, std::vector<Vector3i> &changed, const Vector3i &position, int facing) {
        const Vector3i last = stepped(position, facing + 1);
        const Vector3i first = stepped(position, facing - 1);
        const auto facesSame = [&](const Vector3i &at) {
            const BlockState *state = shelfState(level, at);
            return state != nullptr && shelfFacing(*state) == facing;
        };
        if (isRowType(level, last, ROW_LAST) && facesSame(last))
            setRow(level, changed, last, true, ROW_ALONE);
        if (isRowType(level, last, ROW_MIDDLE) && facesSame(last))
            setRow(level, changed, last, true, ROW_FIRST);
        if (isRowType(level, first, ROW_FIRST) && facesSame(first))
            setRow(level, changed, first, true, ROW_ALONE);
        if (isRowType(level, first, ROW_MIDDLE) && facesSame(first))
            setRow(level, changed, first, true, ROW_LAST);
        setRow(level, changed, position, false, ROW_ALONE);
    }

    ShelfBlockActor *shelfActor(Level &level, const Vector3i &position) {
        return level.getBlockActors().find<ShelfBlockActor>(position);
    }

    // the shelves of the row the clicked one is in, the hotbar thirds they swap with: 0-2, 3-5, 6-8
    std::array<ShelfBlockActor *, 3> rowFor(Level &level, const Vector3i &position, const BlockState &state) {
        std::array<ShelfBlockActor *, 3> row = {};
        const int facing = shelfFacing(state);
        const Vector3i last = stepped(position, facing + 1);
        const Vector3i farLast = stepped(position, facing + 1, 2);
        const Vector3i first = stepped(position, facing - 1);
        const Vector3i farFirst = stepped(position, facing - 1, 2);
        // from the last shelf to the first, every one present and each joining the next
        const auto three = [&](const Vector3i &a, const Vector3i &b, const Vector3i &c) {
            ShelfBlockActor *lastShelf = shelfActor(level, a);
            ShelfBlockActor *middleShelf = shelfActor(level, b);
            ShelfBlockActor *firstShelf = shelfActor(level, c);
            if (lastShelf != nullptr && middleShelf != nullptr && firstShelf != nullptr && joins(level, a, b)
                && joins(level, b, c))
                row = {lastShelf, middleShelf, firstShelf};
        };
        const auto two = [&](const Vector3i &a, const Vector3i &b) {
            ShelfBlockActor *lastShelf = shelfActor(level, a);
            ShelfBlockActor *firstShelf = shelfActor(level, b);
            if (lastShelf != nullptr && firstShelf != nullptr && joins(level, a, b))
                row = {nullptr, lastShelf, firstShelf};
        };
        switch (state.mStates.getInt("powered_shelf_type")) {
            case ROW_ALONE:
                row[2] = shelfActor(level, position);
                break;
            case ROW_FIRST:
                if (isRowType(level, last, ROW_MIDDLE) && isRowType(level, farLast, ROW_LAST))
                    three(farLast, last, position);
                else if (isRowType(level, last, ROW_LAST))
                    two(last, position);
                break;
            case ROW_MIDDLE:
                if (isRowType(level, last, ROW_LAST) && isRowType(level, first, ROW_FIRST))
                    three(last, position, first);
                break;
            case ROW_LAST:
                if (isRowType(level, first, ROW_MIDDLE) && isRowType(level, farFirst, ROW_FIRST))
                    three(position, first, farFirst);
                else if (isRowType(level, first, ROW_FIRST))
                    two(position, first);
                break;
            default:
                break;
        }
        return row;
    }

    // each shelf of the row swaps its three slots with a third of the hotbar; a locked hotbar item stays
    bool swapRow(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position, const BlockState &state) {
        Level &level = owner.getLevelFor(player);
        const std::array<ShelfBlockActor *, 3> row = rowFor(level, position, state);
        if (row[0] == nullptr && row[1] == nullptr && row[2] == nullptr)
            return false;

        PlayerInventory &inventory = player.getInventory();
        bool changed = false;
        bool both = false;
        for (int third = 0; third < 3; ++third) {
            ShelfBlockActor *shelf = row[(size_t) third];
            if (shelf == nullptr)
                continue;
            for (int slot = 0; slot < ShelfBlockActor::SIZE; ++slot) {
                const int hotbar = third * ShelfBlockActor::SIZE + slot;
                const ItemStack held = inventory.getItem(hotbar);
                const ItemStack stored = shelf->getInventory().getContainerItem(slot);
                if ((held.isAir() && stored.isAir()) || ItemProperties::getLockMode(held) != ItemLockMode::None)
                    continue;
                shelf->getInventory().setContainerItem(slot, held);
                inventory.setItem(hotbar, stored);
                player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, hotbar);
                changed = true;
                both = both || (!held.isAir() && !stored.isAir());
            }
            BlockActionHandler::broadcastBlockActorData(owner, level, *shelf);
        }
        const Vector3f center = VectorMath::centerOf(position);
        owner.playLevelSound(level, "multi_swap", center);
        if (changed)
            level.postGameEvent(both ? GameEvents::MULTI_ITEM_SWAP : GameEvents::SINGLE_ITEM_SWAP, center, &player);
        return true;
    }

    // the held item and the one in the slot change places; with instant building the shelf takes a copy and the
    // hand keeps its own
    bool useShelf(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                  const BlockState &state, int face, const Vector3f &clickPosition) {
        // the open containers ability, which every mode but spectator has
        if (player.isSpectator() || face != shelfFront(state))
            return false;
        if (isPowered(state))
            return swapRow(owner, player, position, state);
        const int slot = shelfSlotAt(face, clickPosition);
        if (slot < 0)
            return false;

        Level &level = owner.getLevelFor(player);
        ShelfBlockActor &shelf = level.getBlockActors().getOrCreate<ShelfBlockActor>(position);
        PlayerInventory &inventory = player.getInventory();
        const ItemStack held = inventory.getItemInHand();
        const ItemStack stored = shelf.getInventory().getContainerItem(slot);
        if ((held.isAir() && stored.isAir()) || ItemProperties::getLockMode(held) != ItemLockMode::None)
            return false;

        const bool both = !held.isAir() && !stored.isAir();
        shelf.getInventory().setContainerItem(slot, held);
        if (!player.isCreative()) {
            inventory.setItemInHand(stored);
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                                  inventory.getSelectedSlot());
        }
        const Vector3f center = VectorMath::centerOf(position);
        owner.playLevelSound(level, both && !player.isCreative() ? "single_swap" : "place_item", center);
        BlockActionHandler::broadcastBlockActorData(owner, level, shelf);
        level.postGameEvent(GameEvents::SINGLE_ITEM_SWAP, center, &player);
        return true;
    }
}

ContainerBlock::ContainerBlock(const Block &base) : Block(base) {}

bool ContainerBlock::hearsActorsInside() const {
    const std::string &name = getIdentifier();
    return name == "minecraft:campfire" || name == "minecraft:soul_campfire";
}

void ContainerBlock::onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                                   const BlockState &state) const {
    if (state.mStates.getByte("extinguished") != 0 || actor.isFireImmune()
        || actor.hasEffect(MobEffectId::FireResistance))
        return;

    const float damage = state.mName == "minecraft:soul_campfire" ? 2.0f : 1.0f;
    owner.hurtActor(actor, damage, "death.attack.inFire", position);
}

bool ContainerBlock::onInteractAt(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                  const BlockState &state, int face, const Vector3f &clickPosition) const {
    const ContainerBlockDefinition *definition = findDefinition(state.mName);
    if (definition != nullptr && definition->mKind == ContainerBlockKind::Shelf)
        return useShelf(owner, player, position, state, face, clickPosition);
    return onInteract(owner, player, position, state);
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

bool ContainerBlock::isShulkerBoxItem(const ItemStack &item) {
    if (item.isAir() || item.mDefinition == nullptr)
        return false;

    const ContainerBlockDefinition *definition = findDefinition(std::string(item.mDefinition->getIdentifier()));
    return definition != nullptr && definition->mKind == ContainerBlockKind::ShulkerBox;
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
    if (BlockActor *existing = blockActors.find(position))
        existing->unpackLoot(owner);

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

            const Tag *items = usedItem.mTag.getType() == Tag::Type::Compound ? usedItem.mTag.get(TAG_ITEMS) : nullptr;
            if (items != nullptr)
                shulker->getInventory().loadItems(*items, owner.getCodecContext());
        }
    }

    // the pot item keeps its sherds in its tag
    if (DecoratedPotBlockActor *pot = dynamic_cast<DecoratedPotBlockActor *>(created.get())) {
        const Tag *sherds = usedItem.mTag.isCompound() ? usedItem.mTag.get("sherds") : nullptr;
        if (sherds != nullptr && sherds->isList())
            pot->setSherds(*sherds);
    }

    blockActors.insert(std::move(created));
}

// a shelf placed against a live power source starts powered
void ContainerBlock::onPlacedInLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                     const BlockState &state) const {
    const ContainerBlockDefinition *definition = findDefinition(state.mName);
    if (definition != nullptr && definition->mKind == ContainerBlockKind::Shelf)
        onRedstonePower(owner, level, position, state);
}

void ContainerBlock::onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state) const {
    const ContainerBlockDefinition *definition = findDefinition(state.mName);
    if (definition == nullptr)
        return;

    if (definition->mDropsContentsOnBreak) {
        ContainerBlockActor *actor = level.getBlockActors().find<ContainerBlockActor>(position);
        if (actor != nullptr) {
            actor->unpackLoot(owner);
            const Vector3f dropPosition((float) position.x + 0.5f, (float) position.y + 0.5f,
                                        (float) position.z + 0.5f);
            SimpleContainerInventory &inventory = actor->getInventory();

            for (int slot = 0; slot < inventory.getContainerSize(); ++slot) {
                const ItemStack &item = inventory.getContainerItem(slot);
                if (!item.isAir() && item.mCount > 0) {
                    ItemActorHandler::dropItem(owner, level, dropPosition, item, ItemActorHandler::randomDropMotion(),
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
            const int facing = context.mFace == FACE_DOWN ? FACE_DOWN
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

// shelves close a face picked through the wrong table: south closes the top, west the bottom (BDS 1.26.61)
uint8_t ContainerBlock::getLiquidBlockedFaces(const BlockState &state) const {
    const ContainerBlockDefinition *definition = findDefinition(getIdentifier());
    if (definition == nullptr || definition->mKind != ContainerBlockKind::Shelf)
        return 0;

    const std::string direction = state.mStates.getString("minecraft:cardinal_direction", "south");
    const int face = direction == "east"    ? LiquidFace::NORTH
                     : direction == "north" ? LiquidFace::WEST
                     : direction == "west"  ? LiquidFace::DOWN
                                            : LiquidFace::UP;
    return (uint8_t) (1 << face);
}

// each class reads its container: Hopper, Dispenser, BrewingStand,
// DecoratedPot and Barrel through ChestBlock
int ContainerBlock::getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const {
    const ContainerBlockDefinition *definition = findDefinition(state.mName);
    if (definition == nullptr)
        return -1;

    switch (definition->mKind) {
        case ContainerBlockKind::Barrel:
        case ContainerBlockKind::Hopper:
        case ContainerBlockKind::Dispenser:
        case ContainerBlockKind::Dropper:
        case ContainerBlockKind::BrewingStand:
        case ContainerBlockKind::DecoratedPot:
            return containerSignal(level, position);
        default:
            return -1;
    }
}

// toggle_bit follows the power and locks the hopper
void ContainerBlock::onRedstonePower(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                     const BlockState &state) const {
    const ContainerBlockDefinition *definition = findDefinition(state.mName);
    if (definition != nullptr && definition->mKind == ContainerBlockKind::Shelf) {
        std::vector<Vector3i> changed;
        checkRow(level, changed, position, state);
        const BlockState current = level.getBlockState(position.x, position.y, position.z);
        const bool signal = RedstoneSystem::isGettingPower(owner, level, position);
        if (signal != isPowered(current)) {
            const Vector3f center = VectorMath::centerOf(position);
            owner.playLevelSound(level, signal ? "activate" : "deactivate", center);
            level.postGameEvent(signal ? GameEvents::BLOCK_ACTIVATE : GameEvents::BLOCK_DEACTIVATE, center);
            if (signal)
                powerOn(level, changed, position, shelfFacing(current));
            else
                powerOff(level, changed, position, shelfFacing(current));
        }
        for (const Vector3i &at: changed)
            level.updateAround(at);
        return;
    }
    if (definition == nullptr || definition->mKind != ContainerBlockKind::Hopper)
        return;

    const bool powered = RedstoneSystem::isGettingPower(owner, level, position);
    if ((state.mStates.getByte("toggle_bit", 0) != 0) == powered)
        return;

    Tag states = state.mStates;
    states.putByte("toggle_bit", powered ? 1 : 0);
    level.setBlock(position, BlockState(state.mName, states), false);
}

const char *ContainerBlock::getBlockActorId() const {
    const ContainerBlockDefinition *definition = findDefinition(getIdentifier());
    return definition == nullptr ? nullptr : definition->mBlockActorId;
}
