#include "Network/Handler/ServerNetworkHandler.h"

#include "Actor/ServerPlayer.h"
#include "Inventory/PlayerInventory.h"
#include "Network/Handler/ChunkStreamHandler.h"
#include "Network/Handler/LoginHandler.h"
#include "Protocol/Packets/AddPlayerPacket.h"
#include "Protocol/Packets/MobArmorEquipmentPacket.h"
#include "Protocol/Packets/MobEquipmentPacket.h"
#include "Protocol/Packets/MovePlayerPacket.h"
#include "Protocol/Packets/RemoveActorPacket.h"

#include <cmath>

namespace {
    const float PLAYER_EYE_HEIGHT = 1.62f;
}

bool ServerNetworkHandler::canPlayerSeePlayer(ServerPlayer &viewer, const ServerPlayer &target) const {
    if (&viewer == &target || !viewer.isSpawned() || !target.isSpawned())
        return false;

    if (viewer.getDimension() != target.getDimension())
        return false;

    const Vector3f position = target.getPosition();
    const int64_t hash = ChunkStreamHandler::packChunk((int32_t) std::floor(position.x) >> 4,
                                                       (int32_t) std::floor(position.z) >> 4);

    return viewer.getSentChunks().find(hash) != viewer.getSentChunks().end();
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
    for (auto &viewerEntry: mPlayers) {
        ServerPlayer &viewer = viewerEntry.second;
        std::unordered_set<uint64_t> &visible = viewer.getVisiblePlayers();

        for (auto &targetEntry: mPlayers) {
            ServerPlayer &target = targetEntry.second;
            if (&target == &viewer)
                continue;

            const bool shouldSee = canPlayerSeePlayer(viewer, target);
            const bool seen = visible.find(target.getRuntimeId()) != visible.end();
            if (shouldSee == seen)
                continue;

            if (shouldSee) {
                visible.insert(target.getRuntimeId());
                _sendPlayerSpawn(viewer, target);
            } else {
                visible.erase(target.getRuntimeId());
                if (viewer.isSpawned())
                    _sendPlayerRemove(viewer, target);
            }
        }
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

    for (auto &entry: mPlayers) {
        ServerPlayer &viewer = entry.second;
        if (viewer.getVisiblePlayers().count(player.getRuntimeId()) != 0)
            mNetworkHandler->send(viewer.getNetworkIdentifier(), move, mCodecContext);
    }
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
