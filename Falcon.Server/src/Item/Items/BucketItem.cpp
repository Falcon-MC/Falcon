#include "Item/Items/BucketItem.h"

#include "Actor/ItemActor.h"
#include "Actor/ServerPlayer.h"
#include "Block/BlockData.h"
#include "Block/Blocks/CauldronBlock.h"
#include "Block/Blocks/LavaBlock.h"
#include "Block/Blocks/LiquidBlock.h"
#include "Block/Blocks/WaterBlock.h"
#include "Block/Systems/LiquidPhysicsSystem.h"
#include "Inventory/InventoryManager.h"
#include "Inventory/PlayerInventory.h"
#include "Level/LevelChunk.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/InventoryHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/BlockStateHasher.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Packets/UpdateBlockPacket.h"
#include "Protocol/Types/ItemStack.h"
#include "Protocol/Types/ItemUseTransaction.h"
#include "Protocol/Types/StartGameTypes.h"

#include <string>
#include <vector>

namespace {
    const int BLOCK_FACE_OFFSETS[6][3] = {
            {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}
    };
}

BucketItem::Content BucketItem::getContent(const ItemStack &item) {
    if (item.mDefinition == nullptr)
        return Content::None;

    const std::string &identifier = item.mDefinition->getIdentifier();
    if (identifier == "minecraft:bucket")
        return Content::Empty;
    if (identifier == "minecraft:water_bucket")
        return Content::Water;
    if (identifier == "minecraft:lava_bucket")
        return Content::Lava;
    if (identifier == "minecraft:powder_snow_bucket")
        return Content::PowderSnow;
    return Content::None;
}

bool BucketItem::isBucket(const ItemStack &item) {
    return getContent(item) != Content::None;
}

bool BucketItem::isReplaceable(const BlockState &state) {
    if (state.mName == "minecraft:air")
        return true;

    const BlockData *data = BlockDataTable::find(state.mName.c_str());
    return data != nullptr && !data->mSolid;
}

Vector3i BucketItem::getPlacementPosition(const ItemUseTransaction &transaction,
                                          const BlockState &clickedState) {
    if (isReplaceable(clickedState))
        return transaction.mBlockPosition;

    const int face = transaction.mBlockFace;
    return Vector3i(transaction.mBlockPosition.x + BLOCK_FACE_OFFSETS[face][0],
                    transaction.mBlockPosition.y + BLOCK_FACE_OFFSETS[face][1],
                    transaction.mBlockPosition.z + BLOCK_FACE_OFFSETS[face][2]);
}

BlockState BucketItem::makeLiquidState(Content content) {
    Tag states = Tag::ofCompound();
    states.putInt("liquid_depth", 0);
    return BlockState(content == Content::Lava ? "minecraft:lava" : "minecraft:water", states);
}

const char *BucketItem::getFilledIdentifier(Content content) {
    switch (content) {
        case Content::Water:
            return "minecraft:water_bucket";
        case Content::Lava:
            return "minecraft:lava_bucket";
        case Content::PowderSnow:
            return "minecraft:powder_snow_bucket";
        default:
            return nullptr;
    }
}

bool BucketItem::applyResult(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &heldItem,
                             const char *resultIdentifier) {
    if (resultIdentifier == nullptr)
        return false;

    PlayerInventory &inventory = player.getInventory();
    ItemStack result;
    result.mDefinition = owner.getItemDefinitions().getDefinition(resultIdentifier);
    if (result.mDefinition == nullptr)
        return false;
    result.mCount = 1;

    if (player.getGameType() == (int32_t) GameType::Creative) {
        InventoryHandler::sendHeldItem(owner, player);
        return true;
    }

    ItemStack remainingHeld = heldItem;
    remainingHeld.mCount--;
    if (remainingHeld.mCount > 0) {
        inventory.setItemInHand(remainingHeld);
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                              inventory.getSelectedSlot());

        std::vector<int> touchedSlots;
        const int remaining = inventory.addItemPartial(result, touchedSlots);
        for (const int slot: touchedSlots)
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
        if (remaining > 0) {
            ItemStack overflow = result;
            overflow.mCount = remaining;
            owner.dropItem(owner.getLevelFor(player), player.getPosition(), overflow, Vector3f(),
                           ItemActor::DEFAULT_PICKUP_DELAY);
        }
    } else {
        inventory.setItemInHand(result);
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                              inventory.getSelectedSlot());
    }

    InventoryHandler::sendHeldItem(owner, player);
    return true;
}

void BucketItem::sendBlockState(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
    if (position.y < level.getMinY() || position.y > level.getMaxY())
        return;

    sendBlockUpdate(owner, level, position, level.getBlockState(position.x, position.y, position.z));
}

void BucketItem::sendBlockUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state, uint32_t layer) {
    UpdateBlockPacket update;
    update.mBlockPosition = position;
    update.mRuntimeId = (uint32_t) BlockStateHasher::hash(state.mName, state.mStates);
    update.mFlags = UpdateBlockPacket::Flag::All;
    update.mDataLayer = layer;
    BlockActionHandler::broadcastToViewers(owner, level,
                                           Vector3f((float) position.x + 0.5f,
                                                    (float) position.y + 0.5f,
                                                    (float) position.z + 0.5f),
                                           update);
}

void BucketItem::sendSound(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const char *sound) {
    const Vector3f center((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    owner.playLevelSound(level, sound, center, "");
}

void BucketItem::sendArmSwing(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) {
    ActorEventPacket swing;
    swing.mRuntimeActorId = player.getRuntimeId();
    swing.mEventId = (uint8_t) EntityEventType::ArmSwing;
    swing.mEventData = 0;
    swing.mHasFirePosition = false;
    BlockActionHandler::broadcastToViewers(owner, owner.getLevelFor(player),
                                           Vector3f((float) position.x + 0.5f,
                                                    (float) position.y + 0.5f,
                                                    (float) position.z + 0.5f),
                                           swing);
}

bool BucketItem::isWaterloggable(Level &level, const Vector3i &position) {
    if (position.y < level.getMinY() || position.y > level.getMaxY())
        return false;

    const BlockState state = level.getBlockState(position.x, position.y, position.z);
    return LiquidPhysicsSystem::getWaterloggingLevel(state) > 0;
}

bool BucketItem::use(ServerNetworkHandler &owner, ServerPlayer &player, const ItemUseTransaction &transaction) {
    const ItemStack &heldItem = player.getInventory().getItemInHand();
    const Content content = getContent(heldItem);
    if (content == Content::None)
        return false;

    Level &level = owner.getLevelFor(player);
    const Vector3i clickedPosition = transaction.mBlockPosition;
    const BlockState clickedState = level.getBlockState(clickedPosition.x, clickedPosition.y, clickedPosition.z);
    const LiquidBlock clickedLiquid(clickedState);

    if (content == Content::Empty) {
        if (clickedState.mName == "minecraft:cauldron") {
            const CauldronBlock cauldron(clickedState);
            if (!cauldron.isFull() || cauldron.getLiquid() == CauldronBlock::Liquid::Empty) {
                sendBlockState(owner, level, clickedPosition);
                return false;
            }

            const Content cauldronContent = cauldron.getLiquid() == CauldronBlock::Liquid::Lava
                                           ? Content::Lava
                                           : Content::Water;
            const char *filledIdentifier = getFilledIdentifier(cauldronContent);
            sendArmSwing(owner, player, clickedPosition);
            level.setBlockState(clickedPosition.x, clickedPosition.y, clickedPosition.z,
                                cauldron.withFillLevel(cauldron.getLiquid(), 0));
            sendBlockState(owner, level, clickedPosition);
            const char *sound = cauldronContent == Content::Water
                                ? WaterBlock(clickedState).getBucketFillSound()
                                : LavaBlock(clickedState).getBucketFillSound();
            sendSound(owner, level, clickedPosition, sound);
            applyResult(owner, player, heldItem, filledIdentifier);
            return true;
        }

        if (clickedState.mName == "minecraft:powder_snow") {
            sendArmSwing(owner, player, clickedPosition);
            level.setBlockState(clickedPosition.x, clickedPosition.y, clickedPosition.z, BlockState("minecraft:air"));
            sendBlockState(owner, level, clickedPosition);
            sendSound(owner, level, clickedPosition, "bucket_fill_powder_snow");
            applyResult(owner, player, heldItem, "minecraft:powder_snow_bucket");
            return true;
        }

        const BlockState clickedOverlay = level.getBlockStateAtLayer(clickedPosition.x, clickedPosition.y,
                                                                     clickedPosition.z, 1);
        const LiquidBlock overlayLiquid(clickedOverlay);
        if (!clickedLiquid.isLiquid() && overlayLiquid.isWater() && overlayLiquid.isSource()) {
            sendArmSwing(owner, player, clickedPosition);
            level.setBlockStateAtLayer(clickedPosition.x, clickedPosition.y, clickedPosition.z, 1, BlockState());
            sendBlockUpdate(owner, level, clickedPosition, BlockState(), 1);
            sendSound(owner, level, clickedPosition, WaterBlock(clickedOverlay).getBucketFillSound());
            applyResult(owner, player, heldItem, getFilledIdentifier(Content::Water));
            return true;
        }

        if (!clickedLiquid.isSource() || (!clickedLiquid.isWater() && !clickedLiquid.isLava())) {
            sendBlockState(owner, level, clickedPosition);
            return false;
        }

        const Content filledContent = clickedLiquid.isWater() ? Content::Water : Content::Lava;
        const char *filledIdentifier = getFilledIdentifier(filledContent);
        if (filledIdentifier == nullptr)
            return false;

        sendArmSwing(owner, player, clickedPosition);
        level.setBlockState(clickedPosition.x, clickedPosition.y, clickedPosition.z, BlockState("minecraft:air"));
        sendBlockState(owner, level, clickedPosition);
        const LiquidBlock &source = clickedLiquid;
        const char *sound = filledContent == Content::Water
                            ? WaterBlock(source).getBucketFillSound()
                            : LavaBlock(source).getBucketFillSound();
        sendSound(owner, level, clickedPosition, sound);
        applyResult(owner, player, heldItem, filledIdentifier);
        return true;
    }

    if (clickedState.mName == "minecraft:cauldron") {
        if (content == Content::PowderSnow) {
            sendBlockState(owner, level, clickedPosition);
            return false;
        }
        const CauldronBlock cauldron(clickedState);
        const CauldronBlock::Liquid liquid = content == Content::Lava
                                            ? CauldronBlock::Liquid::Lava
                                            : CauldronBlock::Liquid::Water;
        if (!cauldron.canFill(liquid)) {
            sendBlockState(owner, level, clickedPosition);
            return false;
        }

        sendArmSwing(owner, player, clickedPosition);
        const BlockState filledCauldron = cauldron.withFillLevel(liquid, 6);
        level.setBlockState(clickedPosition.x, clickedPosition.y, clickedPosition.z, filledCauldron);
        sendBlockUpdate(owner, level, clickedPosition, filledCauldron);
        const char *sound = content == Content::Water
                            ? WaterBlock(filledCauldron).getBucketEmptySound()
                            : LavaBlock(filledCauldron).getBucketEmptySound();
        sendSound(owner, level, clickedPosition, sound);
        applyResult(owner, player, heldItem, "minecraft:bucket");
        return true;
    }

    if (content == Content::Water) {
        const int face = transaction.mBlockFace;
        const Vector3i adjacent(clickedPosition.x + BLOCK_FACE_OFFSETS[face][0],
                                clickedPosition.y + BLOCK_FACE_OFFSETS[face][1],
                                clickedPosition.z + BLOCK_FACE_OFFSETS[face][2]);
        const Vector3i waterlogged = isWaterloggable(level, clickedPosition) ? clickedPosition : adjacent;

        if (isWaterloggable(level, waterlogged)) {
            const BlockState overlay = level.getBlockStateAtLayer(waterlogged.x, waterlogged.y, waterlogged.z, 1);
            if (LiquidBlock(overlay).isLiquid()) {
                sendBlockUpdate(owner, level, waterlogged, overlay, 1);
                return false;
            }

            sendArmSwing(owner, player, clickedPosition);
            const BlockState water = makeLiquidState(content);
            level.setBlockStateAtLayer(waterlogged.x, waterlogged.y, waterlogged.z, 1, water);
            sendBlockUpdate(owner, level, waterlogged, water, 1);
            sendSound(owner, level, waterlogged, WaterBlock(water).getBucketEmptySound());
            applyResult(owner, player, heldItem, "minecraft:bucket");
            return true;
        }
    }

    const Vector3i target = getPlacementPosition(transaction, clickedState);
    if (target.y < level.getMinY() || target.y > level.getMaxY()) {
        sendBlockState(owner, level, clickedPosition);
        return false;
    }

    const BlockState targetState = level.getBlockState(target.x, target.y, target.z);
    if (!isReplaceable(targetState)) {
        sendBlockState(owner, level, target);
        return false;
    }

    const LiquidBlock targetLiquid(targetState);
    if (content == Content::PowderSnow && targetLiquid.isLiquid()) {
        sendBlockState(owner, level, target);
        return false;
    }
    if (targetLiquid.isLiquid()
        && targetLiquid.isWater() == (content == Content::Water)) {
        sendBlockState(owner, level, target);
        return false;
    }

    sendArmSwing(owner, player, clickedPosition);

    if (targetLiquid.isLiquid()) {
        const std::string result = content == Content::Water
                                   ? targetLiquid.isSource() ? "minecraft:obsidian"
                                   : targetLiquid.getDecay() <= 4 ? "minecraft:cobblestone" : "minecraft:stone"
                                   : "minecraft:stone";
        const BlockState hardenedState(result);
        level.setBlockState(target.x, target.y, target.z, hardenedState);
        sendBlockUpdate(owner, level, target, hardenedState);
    } else {
        const BlockState placedState = content == Content::PowderSnow
                                       ? BlockState("minecraft:powder_snow")
                                       : makeLiquidState(content);
        level.setBlockState(target.x, target.y, target.z, placedState);
        if (content == Content::Water || content == Content::Lava)
            level.scheduleFluidTick(target, LiquidBlock(placedState).getTickRate());
        sendBlockUpdate(owner, level, target, placedState);
    }

    const char *sound = content == Content::PowderSnow
                       ? "bucket_empty_powder_snow"
                       : content == Content::Water
                         ? WaterBlock(makeLiquidState(content)).getBucketEmptySound()
                         : LavaBlock(makeLiquidState(content)).getBucketEmptySound();
    sendSound(owner, level, target, sound);
    applyResult(owner, player, heldItem, "minecraft:bucket");
    return true;
}
