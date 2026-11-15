#include "Network/ServerNetworkHandler.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/PlayerInventory.h"
#include "Network/Handler/ChunkStreamHandler.h"
#include "Network/Handler/LoginHandler.h"
#include "Protocol/Packets/AddPlayerPacket.h"
#include "Protocol/Packets/MobArmorEquipmentPacket.h"
#include "Protocol/Packets/MobEquipmentPacket.h"
#include "Protocol/Packets/MovePlayerPacket.h"
#include "Protocol/Packets/RemoveActorPacket.h"
#include "World/Actor/Movement/MovementConstants.h"

#include <cmath>

namespace {
    using MovementPhysics::PLAYER_EYE_HEIGHT;
}

bool ServerNetworkHandler::canPlayerSeePlayer(ServerPlayer &viewer, const ServerPlayer &target) {
    if (&viewer == &target || !viewer.isSpawned() || !target.isSpawned())
        return false;

    if (!viewer.sharesLevelWith(target))
        return false;

    const Vector3f position = target.getPosition();
    if (!ChunkStreamHandler::isInActorRange(viewer.getPosition(), position))
        return false;

    return getLevelFor(target).isChunkPopulated((int32_t) std::floor(position.x) >> 4,
                                                (int32_t) std::floor(position.z) >> 4);
}

void ServerNetworkHandler::_sendPlayerSpawn(ServerPlayer &viewer, ServerPlayer &target) {
    const PlayerInventory &inventory = target.getInventory();

    AddPlayerPacket add;
    add.mUuid = LoginHandler::playerListUuid(target);
    add.mUsername = target.getName();
    add.mRuntimeActorId = (int64_t) target.getRuntimeId();
    add.mPosition = target.getPosition();
    add.mMotion = target.getMotion();
    add.mRotation = target.getRotation();
    add.mHand = inventory.getItemInHand();
    add.mGameType = target.getGameType();
    add.mMetadata = _buildPlayerData(target);
    add.mAbilities = LoginHandler::buildAbilities(*this, target);
    add.mBuildPlatform = target.getBuildPlatform();
    mNetworkHandler->send(viewer.getNetworkIdentifier(), add, mCodecContext);

    MobArmorEquipmentPacket armor;
    armor.mRuntimeActorId = (int64_t) target.getRuntimeId();
    armor.mHelmet = inventory.getArmor(PlayerInventory::ARMOR_HEAD);
    armor.mChestplate = inventory.getArmor(PlayerInventory::ARMOR_TORSO);
    armor.mLeggings = inventory.getArmor(PlayerInventory::ARMOR_LEGS);
    armor.mBoots = inventory.getArmor(PlayerInventory::ARMOR_FEET);
    armor.mBody = ItemStack::air();
    mNetworkHandler->send(viewer.getNetworkIdentifier(), armor, mCodecContext);

    MobEquipmentPacket offhand;
    offhand.mRuntimeActorId = (int64_t) target.getRuntimeId();
    offhand.mItem = inventory.getOffhand();
    offhand.mInventorySlot = PlayerInventory::OFFHAND_NETWORK_SLOT;
    offhand.mHotbarSlot = PlayerInventory::OFFHAND_NETWORK_SLOT;
    offhand.mContainerId = PlayerInventory::CONTAINER_ID_OFFHAND;
    mNetworkHandler->send(viewer.getNetworkIdentifier(), offhand, mCodecContext);
}

void ServerNetworkHandler::_sendPlayerRemove(ServerPlayer &viewer, const ServerPlayer &target) {
    RemoveActorPacket remove;
    remove.mUniqueActorId = target.getUniqueId();

    mNetworkHandler->send(viewer.getNetworkIdentifier(), remove, mCodecContext);
}

void ServerNetworkHandler::updatePlayerVisibility() {
    // every offset the range table holds fits in this many chunks
    static const int32_t RANGE_CHUNKS = 4;
    static std::vector<uint64_t> gone;

    mPlayerIndexDirty = true;
    for (auto &viewerEntry: mPlayers) {
        ServerPlayer &viewer = viewerEntry.second;
        std::unordered_set<uint64_t> &visible = viewer.getVisiblePlayers();

        // the ones seen, then the ones near: never every player against every other
        gone.clear();
        for (const uint64_t runtimeId: visible) {
            ServerPlayer *target = getPlayerByRuntimeId(runtimeId);
            if (target == nullptr || !canPlayerSeePlayer(viewer, *target))
                gone.push_back(runtimeId);
        }
        for (const uint64_t runtimeId: gone) {
            visible.erase(runtimeId);
            ServerPlayer *target = getPlayerByRuntimeId(runtimeId);
            if (target != nullptr && viewer.isSpawned())
                _sendPlayerRemove(viewer, *target);
        }

        if (!viewer.isSpawned())
            continue;

        const Vector3f position = viewer.getPosition();
        forEachPlayerAroundChunk((int32_t) std::floor(position.x) >> 4, (int32_t) std::floor(position.z) >> 4,
                                 RANGE_CHUNKS, [&](ServerPlayer &target) {
            if (&target == &viewer || visible.find(target.getRuntimeId()) != visible.end()
                || !canPlayerSeePlayer(viewer, target))
                return;

            visible.insert(target.getRuntimeId());
            _sendPlayerSpawn(viewer, target);
        });
    }
}

void ServerNetworkHandler::broadcastPlayerMove(ServerPlayer &player) {
    if (!player.hasMovedSinceBroadcast())
        return;

    player.markMoveBroadcast();

    MovePlayerPacket move;
    move.mRuntimeActorId = (int64_t) player.getRuntimeId();
    move.mPosition = Vector3f(player.getPosition().x, player.getPosition().y + PLAYER_EYE_HEIGHT,
                              player.getPosition().z);
    move.mRotation = player.getRotation();
    move.mMode = MovePlayerMode::Normal;
    move.mOnGround = player.isOnGround();
    move.mTick = (int64_t) mCurrentTick;

    // a viewer was in range at the last visibility pass: one chunk more covers the move since
    static const int32_t VIEWER_CHUNKS = 6;
    const Vector3f position = player.getPosition();
    forEachPlayerAroundChunk((int32_t) std::floor(position.x) >> 4, (int32_t) std::floor(position.z) >> 4,
                             VIEWER_CHUNKS, [&](ServerPlayer &viewer) {
        if (viewer.getVisiblePlayers().count(player.getRuntimeId()) != 0)
            mNetworkHandler->send(viewer.getNetworkIdentifier(), move, mCodecContext);
    });
}

void ServerNetworkHandler::sendToPlayerViewers(const ServerPlayer &player, const Packet &packet, bool includeSelf) {
    // a viewer was in range at the last visibility pass: one chunk more covers the move since
    static const int32_t VIEWER_CHUNKS = 6;
    static std::vector<NetworkIdentifier> viewers;
    viewers.clear();
    if (includeSelf && player.isSpawned())
        viewers.push_back(player.getNetworkIdentifier());

    const Vector3f position = player.getPosition();
    forEachPlayerAroundChunk((int32_t) std::floor(position.x) >> 4, (int32_t) std::floor(position.z) >> 4,
                             VIEWER_CHUNKS, [&](ServerPlayer &viewer) {
        if (&viewer != &player && viewer.isSpawned() && viewer.getVisiblePlayers().count(player.getRuntimeId()) != 0)
            viewers.push_back(viewer.getNetworkIdentifier());
    });
    mNetworkHandler->sendToMany(viewers, packet, mCodecContext);
}

void ServerNetworkHandler::despawnPlayerForViewers(ServerPlayer &player) {
    for (auto &entry: mPlayers) {
        ServerPlayer &viewer = entry.second;
        if (&viewer == &player)
            continue;

        if (viewer.getVisiblePlayers().erase(player.getRuntimeId()) != 0 && viewer.isSpawned())
            _sendPlayerRemove(viewer, player);
    }
}
