#include "Falcon/Plugin/PluginPackets.h"

#include "World/Actor/Player/ServerPlayer.h"
#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Network/ServerNetworkHandler.h"
#include "Falcon/Plugin/PluginEvent.h"
#include "Falcon/Plugin/PluginManager.h"

#include "Core/Json/Json.h"
#include "Protocol/Packets/LevelEventPacket.h"

#include <algorithm>
#include <cmath>
#include <utility>

uint32_t PluginPackets::worldEffectType(const Packet &packet) {
    switch (packet.getId()) {
        case MinecraftPacketIds::PlaySound:
        case MinecraftPacketIds::LevelSoundEvent:
            return FALCON_EVENT_WORLD_SOUND;
        case MinecraftPacketIds::SpawnParticleEffect:
        case MinecraftPacketIds::LevelEventGeneric:
            return FALCON_EVENT_WORLD_PARTICLE;
        case MinecraftPacketIds::LevelEvent: {
            const auto &event = static_cast<const LevelEventPacket &>(packet);
            const int32_t id = event.mEventId;
            if ((1000 <= id && id <= 1066) || (3512 <= id && id <= 3515))
                return FALCON_EVENT_WORLD_SOUND;
            if ((id & 0x4000) != 0 || (2000 <= id && id <= 2040) || id == 3510 || id == 3511
                || (3603 <= id && id <= 3614) || id == LevelEventPacket::ParticleSmashAttackGroundDust)
                return FALCON_EVENT_WORLD_PARTICLE;
            return 0;
        }
        default:
            return 0;
    }
}

std::vector<NetworkIdentifier> PluginPackets::broadcastWorldEffect(
        ServerNetworkHandler &owner, Level &level, const Vector3f &position, const Packet &packet, uint32_t type,
        std::vector<NetworkIdentifier> viewers) {
    if (PluginManager *plugins = PluginManager::findWithSubscribers(type)) {
        std::string recipients = "[";
        for (const NetworkIdentifier &id: viewers) {
            const auto found = owner.getPlayers().find(id);
            if (found == owner.getPlayers().end())
                continue;
            if (recipients.size() != 1)
                recipients += ',';
            recipients += std::to_string(found->second.getRuntimeId());
        }
        recipients += ']';

        BinaryStream stream;
        packet.write(stream, owner.getCodecContext());
        std::string payload = stream.getBuffer();
        PluginEvent event;
        event.mType = type;
        event.mCancellable = true;
        event.mLevel = &level;
        event.mPosition = position;
        event.mPacketId = (uint32_t) packet.getId();
        event.mPacketData = &payload;
        event.mCustomData = recipients;
        plugins->dispatch(event);
        if (event.mCancelled)
            return {};

        if (event.mCustomData != recipients) {
            const auto list = json::parse(event.mCustomData);
            if (list != nullptr && list->isArray() && list->mArray.size() <= owner.getPlayers().size()) {
                viewers.clear();
                for (const auto &value: list->mArray) {
                    const double id = value->number(-1);
                    if (!std::isfinite(id) || id < 0 || id >= 0x1p64)
                        continue;
                    ServerPlayer *player = owner.getPlayerByRuntimeId((uint64_t) id);
                    if (player == nullptr || !player->isSpawned() || !player->isIn(level))
                        continue;
                    const auto &target = player->getNetworkIdentifier();
                    if (std::find(viewers.begin(), viewers.end(), target) == viewers.end())
                        viewers.push_back(target);
                }
            }
        }
    }
    owner.getNetworkHandler().sendToMany(viewers, packet, owner.getCodecContext());
    return viewers;
}

std::string PluginPackets::encode(uint32_t header, const std::string &payload) {
    BinaryStream stream;
    stream.putUnsignedVarInt(header);
    stream.put(payload);
    return stream.getBuffer();
}

bool PluginPackets::onReceive(PluginManager &manager, const NetworkIdentifier &id, std::string &data) {
    if (!manager.hasSubscribers(FALCON_EVENT_DATA_PACKET_RECEIVE))
        return true;
    return _dispatch(manager, FALCON_EVENT_DATA_PACKET_RECEIVE, id, data);
}

bool PluginPackets::onSend(PluginManager &manager, const NetworkIdentifier &id, std::string &data) {
    if (!manager.hasSubscribers(FALCON_EVENT_DATA_PACKET_SEND))
        return true;
    return _dispatch(manager, FALCON_EVENT_DATA_PACKET_SEND, id, data);
}

bool PluginPackets::_dispatch(PluginManager &manager, uint32_t type, const NetworkIdentifier &id, std::string &data) {
    ReadOnlyBinaryStream stream(data);
    const uint32_t header = stream.getUnsignedVarInt();
    const size_t headerSize = stream.getOffset();

    std::string payload = data.substr(headerSize);
    const std::string original = payload;

    auto &players = manager.getOwner().getPlayers();
    const auto found = players.find(id);

    PluginEvent event;
    event.mType = type;
    event.mCancellable = true;
    event.mPlayer = found == players.end() ? nullptr : &found->second;
    event.mPacketId = header & 0x3ff;
    event.mPacketData = &payload;
    manager.dispatch(event);

    if (event.mCancelled)
        return false;

    if (payload != original)
        data = encode(header, payload);
    return true;
}
