#include "Network/Handler/MapHandler.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/CartographyRecipes.h"
#include "World/Item/MapItem.h"
#include "World/Level/Block/CartographyTableBlock.h"
#include "World/Level/Block/Actor/ItemFrameBlockActor.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/MapCreateLockedCopyPacket.h"
#include "Protocol/Packets/MapInfoRequestPacket.h"

#include <algorithm>
#include <vector>

namespace {
    bool isPlaying(const ServerPlayer &player) {
        return player.getLoginState() >= ServerPlayer::LoginState::StartGameSent;
    }

    void sendUpdate(ServerNetworkHandler &owner, ServerPlayer &player, MapItemSavedData &data) {
        MapItemTrackingInfo *tracking = data.findTracked(MapItemTrackedActorKey::entity(player.getUniqueId()));
        if (tracking == nullptr)
            return;

        ClientboundMapItemDataPacket packet;
        if (data.buildUpdatePacket(*tracking, packet))
            owner.sendPacketTo(player.getNetworkIdentifier(), packet);
    }

    const ItemFrameBlockActor *frameAt(Level &level, const Vector3i &position) {
        if (level.peekChunkPtr(position.x >> 4, position.z >> 4) == nullptr)
            return nullptr;

        return level.getBlockActors().find<ItemFrameBlockActor>(position);
    }
}

void MapHandler::sendMap(ServerNetworkHandler &owner, ServerPlayer &player, MapItemSavedData &data) {
    Level &mapLevel = MapItem::getMapLevel(owner, player);
    const std::vector<int64_t> chain = mapLevel.getMaps().getMapChain(mapLevel.getMapStorage(), data);
    owner.sendPacketTo(player.getNetworkIdentifier(), data.buildCreationPacket(chain));
    owner.sendPacketTo(player.getNetworkIdentifier(), data.buildFullPacket());
}

void MapHandler::broadcastCreation(ServerNetworkHandler &owner, World &world, MapItemSavedData &data) {
    Level &mapLevel = world.getOverworld();
    const ClientboundMapItemDataPacket packet = data.buildCreationPacket(
            mapLevel.getMaps().getMapChain(mapLevel.getMapStorage(), data));
    for (auto &entry: owner.getPlayers()) {
        if (isPlaying(entry.second) && &owner.getWorldFor(entry.second) == &world)
            owner.sendPacketTo(entry.second.getNetworkIdentifier(), packet);
    }
}

void MapHandler::tickPlayer(ServerNetworkHandler &owner, ServerPlayer &player) {
    PlayerInventory &inventory = player.getInventory();
    std::vector<int64_t> carried;

    const auto tickItem = [&](ItemStack &item, bool selected) {
        if (!MapItem::isFilledMap(item))
            return false;

        const bool changed = MapItem::inventoryTick(owner, player, item, selected);
        const int64_t mapId = MapItem::getMapId(item);
        if (mapId != MapItemSavedData::INVALID_ID
            && std::find(carried.begin(), carried.end(), mapId) == carried.end())
            carried.push_back(mapId);
        return changed;
    };

    for (int slot = 0; slot < inventory.getContainerSize(); ++slot) {
        if (!MapItem::isFilledMap(inventory.getItem(slot)))
            continue;

        ItemStack item = inventory.getItem(slot);
        if (tickItem(item, slot == inventory.getSelectedSlot())) {
            inventory.setItem(slot, std::move(item));
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
        }
    }

    // the game draws a map held in the off hand as well
    if (MapItem::isFilledMap(inventory.getOffhand())) {
        ItemStack item = inventory.getOffhand();
        if (tickItem(item, true)) {
            inventory.setOffhand(std::move(item));
            player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Offhand, 0);
        }
    }

    Level &mapLevel = MapItem::getMapLevel(owner, player);
    for (const int64_t mapId: carried) {
        MapItemSavedData *data = mapLevel.getMaps().findLoaded(mapId);
        if (data != nullptr)
            sendUpdate(owner, player, *data);
    }

    for (const int64_t mapId: player.getMapRequests().takeDue()) {
        MapItemSavedData *data = mapLevel.getMapData(mapId);
        if (data == nullptr)
            continue;

        sendMap(owner, player, *data);
        // the client asks once per session, so this is who sees a picture: framed or held
        if (data->isImage())
            player.getMapRequests().markViewing(mapId);
    }
}

void MapHandler::tickFrames(ServerNetworkHandler &owner, World &world) {
    struct FramedMap {
        MapItemSavedData *mData;
        MapItemTrackedActorKey mKey;
        int32_t mDimension;
    };

    std::vector<FramedMap> framed;
    world.getOverworld().getMaps().forEachLoaded([&framed](MapItemSavedData &data) {
        for (const MapItemTrackingInfo &tracking: data.getTracked()) {
            if (tracking.mKey.mType == MapItemTrackedActorKey::Type::Block)
                framed.push_back({&data, tracking.mKey, tracking.mDimension});
        }
    });

    for (const FramedMap &entry: framed) {
        Level &level = world.getLevel(Dimension::fromId(entry.mDimension));
        const ItemFrameBlockActor *frame = frameAt(level, entry.mKey.mPosition);
        if (frame == nullptr)
            continue;

        if (!MapItem::isFilledMap(frame->getItem()) || MapItem::getMapId(frame->getItem()) != entry.mData->getMapId()) {
            entry.mData->removeTracked(entry.mKey);
            continue;
        }

        if (!MapItem::displaysPlayers(frame->getItem()))
            continue;

        MapItem::tickCarriedBy(owner, world, *entry.mData, &frame->getItem());
        MapItemTrackingInfo *tracking = entry.mData->findTracked(entry.mKey);
        ClientboundMapItemDataPacket packet;
        if (tracking == nullptr || !entry.mData->buildUpdatePacket(*tracking, packet))
            continue;

        for (auto &player: owner.getPlayers()) {
            if (isPlaying(player.second) && player.second.isIn(level))
                owner.sendPacketTo(player.second.getNetworkIdentifier(), packet);
        }
    }
}

void MapHandler::onFrameItemChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                    const ItemStack &previous, const ItemStack &current) {
    World &world = owner.getWorldOf(&level);
    Level &mapLevel = world.getOverworld();
    const MapItemTrackedActorKey key = MapItemTrackedActorKey::block(position);

    if (MapItem::isFilledMap(previous)) {
        if (MapItemSavedData *data = mapLevel.getMapData(MapItem::getMapId(previous)))
            data->removeTracked(key);
    }

    if (!MapItem::isFilledMap(current))
        return;

    MapItemSavedData *data = mapLevel.getMapData(MapItem::getMapId(current));
    if (data == nullptr || data->isImage())
        return;

    MapItemTrackingInfo &tracking = data->addTracked(key, level.getDimensionId(), MapDecorationType::MarkerGreen);
    if (!MapItem::updateTrackedDecoration(owner, world, *data, tracking))
        data->removeDecoration(key);
}

void MapHandler::handleInfoRequest(ServerNetworkHandler &owner, ServerPlayer &player,
                                   const MapInfoRequestPacket &packet) {
    // a client running client side generation resamples the chunks it built and sends what the map lacks
    if (!packet.mPixels.empty() && MapItem::isHoldingMap(player, packet.mUniqueMapId)) {
        Level &mapLevel = MapItem::getMapLevel(owner, player);
        if (MapItemSavedData *data = mapLevel.getMapData(packet.mUniqueMapId))
            data->applyClientPixels(packet.mPixels);
        return;
    }

    // the game answers at once; queued here: a flood of ids costs a few reads and sends per tick at most
    player.getMapRequests().push(packet.mUniqueMapId);
}

void MapHandler::handleCreateLockedCopy(ServerNetworkHandler &owner, ServerPlayer &player,
                                        const MapCreateLockedCopyPacket &packet) {
    // the game copies for any holder; here only the id is kept for the lock the open table is about to craft:
    // the item stack request makes the copy, never this packet alone
    InventoryManager &manager = player.getInventoryManager();
    if (!manager.isCartographyOpen() || packet.mNewMapId == MapItemSavedData::INVALID_ID)
        return;

    const Vector3i &position = manager.getAnvilPosition();
    if (owner.getLevelFor(player).getBlockState(position.x, position.y, position.z).mName
        != CartographyTableBlock::IDENTIFIER)
        return;

    const PlayerInventory &inventory = player.getInventory();
    if (!CartographyRecipes::isLocking(inventory.getCartographyItem(PlayerInventory::CARTOGRAPHY_INPUT),
                                       inventory.getCartographyItem(PlayerInventory::CARTOGRAPHY_ADDITIONAL),
                                       packet.mOriginalMapId))
        return;

    CartographyLock lock;
    lock.mOriginalId = packet.mOriginalMapId;
    lock.mCopyId = packet.mNewMapId;
    manager.setClientMapLock(lock);
}
