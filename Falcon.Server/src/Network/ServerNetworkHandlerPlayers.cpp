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

    if (viewer.hidesPlayer(target) || !viewer.sharesLevelWith(target))
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
    armor.mBody = inventory.getBodyArmor();
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

void ServerNetworkHandler::setPlayerVisibleTo(ServerPlayer &viewer, ServerPlayer &target, bool visible) {
    if (&viewer == &target)
        return;
    mPlayerVisibility.erase(viewer.getRuntimeId());
    viewer.setPlayerHidden(target, !visible);
    if (visible)
        spawnPlayerTo(viewer, target);
    else
        despawnPlayerFrom(viewer, target, true);
}

void ServerNetworkHandler::spawnPlayerTo(ServerPlayer &viewer, ServerPlayer &target) {
    mPlayerVisibility.erase(viewer.getRuntimeId());
    if (canPlayerSeePlayer(viewer, target) && viewer.getVisiblePlayers().insert(target.getRuntimeId()).second)
        _sendPlayerSpawn(viewer, target);
}

void ServerNetworkHandler::despawnPlayerFrom(ServerPlayer &viewer, const ServerPlayer &target, bool sendPacket) {
    mPlayerVisibility.erase(viewer.getRuntimeId());
    if (viewer.getVisiblePlayers().erase(target.getRuntimeId()) != 0 && viewer.isSpawned() && sendPacket)
        _sendPlayerRemove(viewer, target);
}

void ServerNetworkHandler::updatePlayerVisibility() {
    static const int32_t RANGE_CHUNKS = 4;
    std::unordered_map<uint64_t, PlayerVisibilityState> current;
    current.reserve(mPlayers.size());
    std::vector<uint64_t> changed;
    std::vector<uint64_t> removed;
    mPlayerIndexDirty = true;

    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;
        const Vector3f position = player.getPosition();
        const int32_t x = (int32_t) std::floor(position.x) >> 4;
        const int32_t z = (int32_t) std::floor(position.z) >> 4;
        PlayerVisibilityState state{&player, player.getWorldId(), player.getDimension(), x, z, player.isSpawned(),
                                    player.isSpawned() && getLevelFor(player).isChunkPopulated(x, z),
                                    player.getUuid(), player.getVisiblePlayers().size()};
        const auto previous = mPlayerVisibility.find(player.getRuntimeId());
        if (previous == mPlayerVisibility.end() || !state.sameLocation(previous->second))
            changed.push_back(player.getRuntimeId());
        current.emplace(player.getRuntimeId(), std::move(state));
    }
    for (const auto &entry: mPlayerVisibility) {
        if (current.find(entry.first) == current.end())
            removed.push_back(entry.first);
    }

    std::vector<uint64_t> gone;
    for (auto &entry: current) {
        PlayerVisibilityState &state = entry.second;
        ServerPlayer &viewer = *state.mPlayer;
        auto &visible = viewer.getVisiblePlayers();
        const auto previous = mPlayerVisibility.find(entry.first);
        const bool refresh = previous == mPlayerVisibility.end() || !state.sameLocation(previous->second)
                             || state.mVisibleCount != previous->second.mVisibleCount;
        for (const uint64_t id: removed)
            visible.erase(id);

        const auto update = [&](const PlayerVisibilityState &target) {
            const uint64_t id = target.mPlayer->getRuntimeId();
            if (canPlayerSeePlayer(viewer, *target.mPlayer)) {
                if (visible.insert(id).second)
                    _sendPlayerSpawn(viewer, *target.mPlayer);
            } else if (visible.erase(id) != 0 && viewer.isSpawned()) {
                _sendPlayerRemove(viewer, *target.mPlayer);
            }
        };

        if (refresh) {
            gone.clear();
            for (const uint64_t id: visible) {
                const auto target = current.find(id);
                if (target == current.end() || !canPlayerSeePlayer(viewer, *target->second.mPlayer))
                    gone.push_back(id);
            }
            for (const uint64_t id: gone) {
                visible.erase(id);
                const auto target = current.find(id);
                if (target != current.end() && viewer.isSpawned())
                    _sendPlayerRemove(viewer, *target->second.mPlayer);
            }
            if (viewer.isSpawned()) {
                forEachPlayerAroundChunk(state.mChunkX, state.mChunkZ, RANGE_CHUNKS, [&](ServerPlayer &target) {
                    if (visible.count(target.getRuntimeId()) != 0)
                        return;
                    const auto found = current.find(target.getRuntimeId());
                    if (found != current.end())
                        update(found->second);
                    else
                        spawnPlayerTo(viewer, target);
                });
            }
        } else {
            for (const uint64_t id: changed)
                update(current.at(id));
        }
        state.mVisibleCount = visible.size();
    }
    mPlayerVisibility = std::move(current);
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

    sendToActorViewers(player, move, false);
}

void ServerNetworkHandler::sendToActorViewers(const Actor &actor, const Packet &packet, bool includeSelf) {
    const bool individualHooks = mNetworkHandler->hasSendHook();
    std::vector<NetworkIdentifier> recipients;
    if (!individualHooks)
        recipients.reserve(mPlayers.size());
    const auto consider = [&](ServerPlayer &viewer) {
        if (!viewer.isSpawned() || !viewer.sharesLevelWith(actor))
            return;
        if (&viewer == &actor) {
            if (!includeSelf)
                return;
        } else {
            const auto &visible = actor.isPlayer() ? viewer.getVisiblePlayers() : viewer.getVisibleActors();
            if (visible.count(actor.getRuntimeId()) == 0)
                return;
        }
        if (individualHooks)
            mNetworkHandler->send(viewer.getNetworkIdentifier(), packet, mCodecContext);
        else
            recipients.push_back(viewer.getNetworkIdentifier());
    };
    if (actor.isPlayer()) {
        // includes the margin used by movement broadcasts since the last visibility pass
        const Vector3f position = actor.getPosition();
        forEachPlayerAroundChunk((int32_t) std::floor(position.x) >> 4, (int32_t) std::floor(position.z) >> 4,
                                 6, consider);
    } else {
        // autonomous actors can remain visible outside the normal chunk range
        for (auto &entry: mPlayers)
            consider(entry.second);
    }
    if (!individualHooks)
        mNetworkHandler->sendToMany(recipients, packet, mCodecContext);
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
