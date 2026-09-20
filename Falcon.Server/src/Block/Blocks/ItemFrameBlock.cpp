#include "Block/Blocks/ItemFrameBlock.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(ItemFrameBlock, 60);

#include "Actor/ServerPlayer.h"
#include "Block/Actor/ItemFrameBlockActor.h"
#include "Block/BlockActorStore.h"
#include "Block/BlockData.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/BlockActorDataPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Types/ItemDefinition.h"
#include "Protocol/Types/StartGameTypes.h"

#include <cstdlib>
#include <utility>

namespace {
    const char *FRAME = "minecraft:frame";
    const char *GLOW_FRAME = "minecraft:glow_frame";
    const char *FILLED_MAP = "minecraft:filled_map";
    const char *STATE_MAP_BIT = "item_frame_map_bit";

    const int FACE_OFFSETS[6][3] = {
            {0,  -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}
    };

    Vector3f centreOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }

    bool rollsDrop(float chance) {
        if (chance >= 1.0f)
            return true;
        if (chance <= 0.0f)
            return false;

        return (float) rand() / (float) RAND_MAX <= chance;
    }

    bool isFilledMap(const ItemStack &item) {
        return item.mDefinition != nullptr && item.mDefinition->getIdentifier() == FILLED_MAP;
    }

    void setStoringMap(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state,
                       bool storingMap) {
        if (!state.mStates.contains(STATE_MAP_BIT))
            return;

        const int8_t wanted = storingMap ? 1 : 0;
        if (state.mStates.getByte(STATE_MAP_BIT, 0) == wanted)
            return;

        Tag states = state.mStates;
        states.putByte(STATE_MAP_BIT, wanted);

        const BlockState updated(state.mName, states);
        level.setBlockState(position.x, position.y, position.z, updated);
        BlockActionHandler::broadcastBlockUpdate(owner, level, position, updated);
    }

    void broadcastFrame(ServerNetworkHandler &owner, Level &level, const ItemFrameBlockActor &frame) {
        BlockActorDataPacket data;
        data.mBlockPosition = frame.getPosition();
        data.mData = frame.getSpawnCompound();
        BlockActionHandler::broadcastToViewers(owner, level, centreOf(frame.getPosition()), data);
    }

    void dropFramedItem(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                        const ItemFrameBlockActor &frame) {
        if (frame.isEmpty() || !rollsDrop(frame.getDropChance()))
            return;

        const Vector3f dropPosition((float) position.x + 0.5f, (float) position.y + 0.25f,
                                    (float) position.z + 0.5f);
        owner.dropItem(level, dropPosition, frame.getItem(), ItemActorHandler::randomDropMotion(),
                       ItemActorHandler::DROP_PICKUP_DELAY);
    }
}

ItemFrameBlock::ItemFrameBlock(const Block &base) : Block(base) {
}

BlockState ItemFrameBlock::applyPlacementOrientation(const BlockState &state,
                                                     const BlockPlacementContext &context) const {
    BlockState result = Block::applyPlacementOrientation(state, context);
    Tag states = result.mStates;

    if (states.contains("facing_direction"))
        states.putInt("facing_direction", context.mFace);

    if (states.contains("minecraft:facing_direction"))
        states.putString("minecraft:facing_direction", PlacementOrientation::faceName(context.mFace));

    return BlockState(result.mName, states);
}

bool ItemFrameBlock::matches(const std::string &identifier) {
    return identifier == FRAME || identifier == GLOW_FRAME;
}

bool ItemFrameBlock::isGlow(const std::string &identifier) {
    return identifier == GLOW_FRAME;
}

std::unique_ptr<BlockActor> ItemFrameBlock::createBlockActor(const std::string &identifier) {
    if (isGlow(identifier))
        return std::unique_ptr<BlockActor>(new GlowItemFrameBlockActor());

    return std::unique_ptr<BlockActor>(new ItemFrameBlockActor());
}

bool ItemFrameBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    if (blockFace < 0 || blockFace >= 6)
        return false;

    const Vector3i support(position.x - FACE_OFFSETS[blockFace][0],
                           position.y - FACE_OFFSETS[blockFace][1],
                           position.z - FACE_OFFSETS[blockFace][2]);

    const BlockState state = level.getBlockState(support.x, support.y, support.z);
    const BlockData *data = BlockDataTable::find(state.mName.c_str());
    const bool solid = data != nullptr && data->mSolid;

    if (matches(state.mName))
        return false;

    return solid;
}

bool ItemFrameBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                const BlockState &state) const {
    Level &level = owner.getLevelFor(player);
    BlockActorStore &blockActors = level.getBlockActors();
    ItemFrameBlockActor *frame = blockActors.find<ItemFrameBlockActor>(position);
    if (frame == nullptr) {
        std::unique_ptr<BlockActor> created = createBlockActor(state.mName);
        created->setPosition(position);
        created->setState(state);
        frame = static_cast<ItemFrameBlockActor *>(created.get());
        blockActors.insert(std::move(created));
    }

    PlayerInventory &inventory = player.getInventory();

    if (frame->isEmpty()) {
        const ItemStack held = inventory.getItemInHand();
        if (held.isAir() || held.mCount <= 0)
            return false;

        ItemStack framed = held;
        framed.mCount = 1;
        frame->setItem(std::move(framed));
        frame->setRotation(0);

        if (player.getGameType() != (int32_t) GameType::Creative) {
            ItemStack remaining = held;
            remaining.mCount -= 1;
            inventory.setItemInHand(remaining.mCount <= 0 ? ItemStack::air() : std::move(remaining));
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                                  inventory.getSelectedSlot());
        }

        setStoringMap(owner, level, position, state, isFilledMap(held));
        owner.playLevelSound(level, LevelSoundEvent::ITEM_FRAME_ADD_ITEM, centreOf(position));
    } else {
        frame->setRotation(frame->getRotation() + 1);
        setStoringMap(owner, level, position, state, false);
        owner.playLevelSound(level, LevelSoundEvent::ITEM_FRAME_ROTATE_ITEM, centreOf(position));
    }

    broadcastFrame(owner, level, *frame);
    return true;
}

bool ItemFrameBlock::onPunch(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                             const BlockState &state) const {
    Level &level = owner.getLevelFor(player);
    ItemFrameBlockActor *frame = level.getBlockActors().find<ItemFrameBlockActor>(position);
    if (frame == nullptr || frame->isEmpty())
        return false;

    const bool creative = player.getGameType() == (int32_t) GameType::Creative;
    if (!creative)
        dropFramedItem(owner, level, position, *frame);

    frame->setItem(ItemStack::air());
    frame->setRotation(0);
    setStoringMap(owner, level, position, state, false);

    owner.playLevelSound(level,
                         creative ? LevelSoundEvent::ITEM_FRAME_REMOVE_ITEM : LevelSoundEvent::ITEM_FRAME_BREAK,
                         centreOf(position));

    broadcastFrame(owner, level, *frame);
    return true;
}

void ItemFrameBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                              const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    (void) usedItem;
    (void) blockFace;

    Level &level = owner.getLevelFor(player);
    BlockActorStore &blockActors = level.getBlockActors();
    blockActors.remove(position);

    std::unique_ptr<BlockActor> created = createBlockActor(state.mName);
    created->setPosition(position);
    created->setState(state);
    blockActors.insert(std::move(created));

    owner.playLevelSound(level, LevelSoundEvent::ITEM_FRAME_PLACE, centreOf(position));
}

void ItemFrameBlock::onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state) const {
    (void) state;

    ItemFrameBlockActor *frame = level.getBlockActors().find<ItemFrameBlockActor>(position);
    if (frame == nullptr)
        return;

    dropFramedItem(owner, level, position, *frame);
    level.getBlockActors().remove(position);

    owner.playLevelSound(level, LevelSoundEvent::ITEM_FRAME_BREAK, centreOf(position));
}
