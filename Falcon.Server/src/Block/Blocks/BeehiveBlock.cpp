#include "Block/Blocks/BeehiveBlock.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Block/Actor/BeehiveBlockActor.h"
#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/ContainerBlock.h"
#include "Block/Components/PlacementOrientation.h"
#include "Item/Items/BucketItem.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/ItemStack.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>

FALCON_REGISTER_BLOCK(BeehiveBlock, 188);

namespace {
    const char *const HONEY_LEVEL = "honey_level";
    const char *const DIRECTION = "direction";
    const char *const RETURNED_TO_HIVE_EVENT = "minecraft:bee_returned_to_hive";
    const char *const HIVE_FULL_EVENT = "minecraft:hive_full";
    const char *const BLOCK_ENTITY_TAG = "BlockEntityTag";
    const char *const GLASS_BOTTLE = "minecraft:glass_bottle";
    const char *const HONEY_BOTTLE = "minecraft:honey_bottle";
    const char *const HONEYCOMB = "minecraft:honeycomb";
    const char *const SHEAR_SOUND = "block.beehive.shear";
    const char *const BOTTLE_FILL_SOUND = "bottle.fill";
    const int32_t SHEARED_HONEYCOMBS = 3;
    const int DIRECTION_FACES[] = {PlacementOrientation::FACE_SOUTH, PlacementOrientation::FACE_WEST,
                                   PlacementOrientation::FACE_NORTH, PlacementOrientation::FACE_EAST};

    Vector3f blockCenter(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }

    bool isCalmedBySmoke(Level &level, const Vector3i &position) {
        const BlockState *below = level.peekBlockPtr(position.x, position.y - 1, position.z);
        const ContainerBlockDefinition *definition = below == nullptr ? nullptr
                                                                      : ContainerBlock::findDefinition(below->mName);
        return definition != nullptr && definition->mKind == ContainerBlockKind::Campfire;
    }

    bool isGlassBottle(const ItemStack &item) {
        return item.mDefinition != nullptr && item.mDefinition->getIdentifier() == GLASS_BOTTLE;
    }
}

bool BeehiveBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:bee_nest" || identifier == "minecraft:beehive";
}

int32_t BeehiveBlock::getHoneyLevel(const BlockState &state) {
    return state.mStates.getInt(HONEY_LEVEL, 0);
}

void BeehiveBlock::setHoneyLevel(Level &level, const Vector3i &position, int32_t honeyLevel) {
    const BlockState *current = level.peekBlockPtr(position.x, position.y, position.z);
    if (current == nullptr)
        return;

    BlockState state = *current;
    state.mStates.putInt(HONEY_LEVEL, std::clamp(honeyLevel, 0, MAX_HONEY_LEVEL));
    level.setBlock(position, state, true);
}

int BeehiveBlock::getFrontFace(const BlockState &state) {
    return DIRECTION_FACES[state.mStates.getInt(DIRECTION, 0) & 3];
}

bool BeehiveBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                              const BlockState &state) const {
    if (getHoneyLevel(state) < MAX_HONEY_LEVEL)
        return false;

    const ItemStack held = player.getInventory().getItemInHand();
    Level &level = owner.getLevelFor(player);
    const Vector3f center = blockCenter(position);

    if (Block::isShears(held)) {
        for (int32_t count = 0; count < SHEARED_HONEYCOMBS; ++count)
            owner.spawnItemActor(level, HONEYCOMB, 1, center);
        owner.playLevelSound(level, SHEAR_SOUND, center);
        owner.damagePlayerHeldItem(player, 1);
    } else if (isGlassBottle(held)) {
        if (!BucketItem::applyResult(owner, player, held, HONEY_BOTTLE))
            return false;
        owner.playLevelSound(level, BOTTLE_FILL_SOUND, center);
    } else {
        return false;
    }

    setHoneyLevel(level, position, 0);

    if (owner.getProperties().getDifficulty() == Difficulty::Peaceful
        || player.getGameType() == (int32_t) GameType::Creative || isCalmedBySmoke(level, position))
        return true;

    BeehiveBlockActor *hive = level.getBlockActors().find<BeehiveBlockActor>(position);
    if (hive != nullptr)
        hive->evacuate(owner, true);
    return true;
}

void BeehiveBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                            const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    (void) state;
    (void) usedItem;
    (void) blockFace;

    BlockActorStore &blockActors = owner.getLevelFor(player).getBlockActors();
    blockActors.remove(position);
    blockActors.getOrCreate<BeehiveBlockActor>(position);
}

void BeehiveBlock::onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const {
    (void) state;

    BeehiveBlockActor *hive = level.getBlockActors().find<BeehiveBlockActor>(position);
    if (hive == nullptr)
        return;

    hive->evacuate(owner, false);
    level.getBlockActors().remove(position);
}

bool BeehiveBlock::onActorEvent(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                const BlockState &state, const std::string &event, MobActor &source) const {
    (void) state;

    if (event != RETURNED_TO_HIVE_EVENT || !level.getBlockActors().isChunkLoaded(position.x >> 4, position.z >> 4))
        return false;

    BeehiveBlockActor &hive = level.getBlockActors().getOrCreate<BeehiveBlockActor>(position);
    if (hive.isFull()) {
        source.fireEvent(owner, HIVE_FULL_EVENT);
        return true;
    }

    return hive.admit(owner, source);
}

void BeehiveBlock::writeDropContents(Level &level, const Vector3i &position, ItemStack &drop) const {
    BeehiveBlockActor *hive = level.getBlockActors().find<BeehiveBlockActor>(position);
    if (hive == nullptr || hive->isEmpty())
        return;

    if (!drop.mTag.isCompound())
        drop.mTag = Tag::ofCompound();

    drop.mTag.put(BLOCK_ENTITY_TAG, hive->saveNbt());
    hive->clearOccupants();
}
