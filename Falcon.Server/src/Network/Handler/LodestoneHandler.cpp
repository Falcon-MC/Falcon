#include "Network/Handler/LodestoneHandler.h"

#include "Network/Handler/InventoryHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/PositionTrackingDBClientRequestPacket.h"
#include "Protocol/Packets/PositionTrackingDBServerBroadcastPacket.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/EnchantmentData.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/LodestoneBlockActor.h"
#include "World/Level/Level.h"
#include "World/Level/PositionTrackingDB.h"
#include "World/Level/World.h"

#include <cmath>
#include <cstdlib>
#include <vector>

namespace {
    const char *const COMPASS = "minecraft:compass";
    const char *const LODESTONE_COMPASS = "minecraft:lodestone_compass";
    const char *const TAG_TRACKING_HANDLE = "trackingHandle";
    const char *const SOUND_LINK = "lodestone_compass.link_compass_to_lodestone";

    bool isPlaying(const ServerPlayer &player) {
        return player.getLoginState() >= ServerPlayer::LoginState::StartGameSent;
    }

    bool isCompassFor(const ItemStack &item, int32_t trackingId) {
        return !item.isAir() && item.mDefinition != nullptr && item.mDefinition->getIdentifier() == LODESTONE_COMPASS
               && item.mTag.isCompound()
               && item.mTag.getInt(TAG_TRACKING_HANDLE, PositionTrackingDB::INVALID_ID) == trackingId;
    }

    void sendRecord(ServerNetworkHandler &owner, const ServerPlayer &player, const TrackingRecord &record) {
        PositionTrackingDBServerBroadcastPacket packet;
        packet.mAction = record.mStatus == TrackingRecordStatus::Destroyed ? PositionTrackingDBBroadcastAction::Destroy
                                                                            : PositionTrackingDBBroadcastAction::Update;
        packet.mTrackingId = record.mId;
        packet.mTag = PositionTrackingDB::toTag(record);
        owner.sendPacketTo(player.getNetworkIdentifier(), packet);
    }

    // the lodestone gets a record on its first link, the compass its id
    void link(ServerNetworkHandler &owner, Level &level, const Vector3i &position, ItemStack &compass) {
        LodestoneBlockActor &lodestone = level.getBlockActors().getOrCreate<LodestoneBlockActor>(position);
        if (!compass.mTag.isCompound())
            compass.mTag = Tag::ofCompound();

        const int32_t handle = compass.mTag.getInt(TAG_TRACKING_HANDLE, PositionTrackingDB::INVALID_ID);
        if (handle == PositionTrackingDB::INVALID_ID || handle != lodestone.mTrackingId) {
            if (lodestone.mTrackingId == PositionTrackingDB::INVALID_ID) {
                World &world = owner.getWorldOf(level);
                const TrackingRecord &record = world.getPositionTracking().track(position, level.getDimensionId());
                lodestone.mTrackingId = record.mId;
                LodestoneHandler::broadcast(owner, world, record);
            }
            compass.mTag.putInt(TAG_TRACKING_HANDLE, lodestone.mTrackingId);
        }

        owner.playLevelSound(level, SOUND_LINK,
                             Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f));
    }
}

bool LodestoneHandler::useCompassOn(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) {
    PlayerInventory &inventory = player.getInventory();
    const ItemStack held = inventory.getItemInHand();
    if (held.isAir() || held.mDefinition == nullptr)
        return false;

    Level &level = owner.getLevelFor(player);
    const std::string &identifier = held.mDefinition->getIdentifier();
    if (identifier == LODESTONE_COMPASS) {
        ItemStack relinked = held;
        link(owner, level, position, relinked);
        inventory.setItemInHand(std::move(relinked));
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, inventory.getSelectedSlot());
        return true;
    }
    if (identifier != COMPASS)
        return false;

    // a new lodestone compass keeping the vanishing curse and the custom name
    ItemStack compass = owner.createItemStack(LODESTONE_COMPASS, 1);
    if (compass.isAir())
        return false;
    compass.mTag = Tag::ofCompound();
    const int32_t vanishing = ItemEnchantments::getLevel(held, EnchantmentIds::VANISHING);
    if (vanishing > 0)
        ItemEnchantments::write(compass, {{EnchantmentIds::VANISHING, vanishing}});
    const Tag *display = held.mTag.isCompound() ? held.mTag.get("display") : nullptr;
    if (display != nullptr && display->isCompound() && display->contains("Name", Tag::Type::String)) {
        Tag name = Tag::ofCompound();
        name.putString("Name", display->getString("Name"));
        compass.mTag.put("display", name);
    }
    link(owner, level, position, compass);

    const bool creative = player.isCreative();
    ItemStack remaining = held;
    if (!creative)
        remaining.mCount -= 1;

    if (creative || remaining.mCount > 0) {
        if (!creative) {
            inventory.setItemInHand(remaining);
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                                  inventory.getSelectedSlot());
        }

        std::vector<int> touched;
        if (!inventory.addItem(compass, touched))
            owner._throwItem(player, compass);
        for (const int slot: touched)
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
    } else {
        inventory.setItemInHand(compass);
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, inventory.getSelectedSlot());
    }

    InventoryHandler::sendHeldItem(owner, player);
    return true;
}

void LodestoneHandler::release(ServerNetworkHandler &owner, Level &level, int32_t trackingId) {
    World &world = owner.getWorldOf(level);
    const TrackingRecord *record = world.getPositionTracking().destroy(trackingId);
    if (record != nullptr)
        broadcast(owner, world, *record);
}

void LodestoneHandler::handleRequest(ServerNetworkHandler &owner, ServerPlayer &player,
                                     const PositionTrackingDBClientRequestPacket &packet) {
    if (packet.mAction != PositionTrackingDBRequestAction::Query)
        return;

    World &world = owner.getWorldFor(player);
    const TrackingRecord *record = world.getPositionTracking().find(packet.mTrackingId);
    // a record the player may not know reads as missing, so its existence leaks nothing either
    if (record != nullptr && record->mStatus != TrackingRecordStatus::Pending && mayKnow(owner, player, *record)) {
        sendRecord(owner, player, *record);
        return;
    }

    PositionTrackingDBServerBroadcastPacket notFound;
    notFound.mAction = PositionTrackingDBBroadcastAction::NotFound;
    notFound.mTrackingId = packet.mTrackingId;
    notFound.mTag = Tag::ofCompound();
    owner.sendPacketTo(player.getNetworkIdentifier(), notFound);
}

bool LodestoneHandler::mayKnow(ServerNetworkHandler &owner, ServerPlayer &player, const TrackingRecord &record) {
    const PlayerInventory &inventory = player.getInventory();
    if (isCompassFor(inventory.getOffhand(), record.mId) || isCompassFor(inventory.getCursor(), record.mId))
        return true;
    for (int slot = 0; slot < inventory.getContainerSize(); ++slot) {
        if (isCompassFor(inventory.getItem(slot), record.mId))
            return true;
    }

    if (owner.getLevelFor(player).getDimensionId() != record.mDimension)
        return false;
    const Vector3f &position = player.getPosition();
    const int32_t radius = player.getChunkStreamState().mRadius;
    const int32_t dx = (record.mPosition.x >> 4) - ((int32_t) std::floor(position.x) >> 4);
    const int32_t dz = (record.mPosition.z >> 4) - ((int32_t) std::floor(position.z) >> 4);
    return std::abs(dx) <= radius && std::abs(dz) <= radius;
}

void LodestoneHandler::broadcast(ServerNetworkHandler &owner, World &world, const TrackingRecord &record) {
    if (record.mStatus == TrackingRecordStatus::Pending)
        return;

    for (auto &entry: owner.getPlayers()) {
        if (isPlaying(entry.second) && &owner.getWorldFor(entry.second) == &world
            && mayKnow(owner, entry.second, record))
            sendRecord(owner, entry.second, record);
    }
}
