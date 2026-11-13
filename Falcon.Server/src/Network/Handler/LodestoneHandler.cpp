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

#include <vector>

namespace {
    const char *const COMPASS = "minecraft:compass";
    const char *const LODESTONE_COMPASS = "minecraft:lodestone_compass";
    const char *const TAG_TRACKING_HANDLE = "trackingHandle";
    const char *const SOUND_LINK = "lodestone_compass.link_compass_to_lodestone";

    bool isPlaying(const ServerPlayer &player) {
        return player.getLoginState() >= ServerPlayer::LoginState::StartGameSent;
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

void LodestoneHandler::destroy(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
    LodestoneBlockActor *lodestone = level.getBlockActors().find<LodestoneBlockActor>(position);
    if (lodestone == nullptr || lodestone->mTrackingId == PositionTrackingDB::INVALID_ID)
        return;

    World &world = owner.getWorldOf(level);
    const TrackingRecord *record = world.getPositionTracking().destroy(lodestone->mTrackingId);
    lodestone->mTrackingId = PositionTrackingDB::INVALID_ID;
    if (record != nullptr)
        broadcast(owner, world, *record);
}

void LodestoneHandler::handleRequest(ServerNetworkHandler &owner, ServerPlayer &player,
                                     const PositionTrackingDBClientRequestPacket &packet) {
    if (packet.mAction != PositionTrackingDBRequestAction::Query)
        return;

    World &world = owner.getWorldFor(player);
    const TrackingRecord *record = world.getPositionTracking().find(packet.mTrackingId);
    if (record != nullptr) {
        broadcast(owner, world, *record);
        return;
    }

    PositionTrackingDBServerBroadcastPacket notFound;
    notFound.mAction = PositionTrackingDBBroadcastAction::NotFound;
    notFound.mTrackingId = packet.mTrackingId;
    notFound.mTag = Tag::ofCompound();
    owner.sendPacketTo(player.getNetworkIdentifier(), notFound);
}

void LodestoneHandler::broadcast(ServerNetworkHandler &owner, World &world, const TrackingRecord &record) {
    if (record.mStatus == TrackingRecordStatus::Pending)
        return;

    PositionTrackingDBServerBroadcastPacket packet;
    packet.mAction = record.mStatus == TrackingRecordStatus::Destroyed ? PositionTrackingDBBroadcastAction::Destroy
                                                                        : PositionTrackingDBBroadcastAction::Update;
    packet.mTrackingId = record.mId;
    packet.mTag = PositionTrackingDB::toTag(record);
    for (auto &entry: owner.getPlayers()) {
        if (isPlaying(entry.second) && &owner.getWorldFor(entry.second) == &world)
            owner.sendPacketTo(entry.second.getNetworkIdentifier(), packet);
    }
}
