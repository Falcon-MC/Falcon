#include "Plugin/PluginPackets.h"

#include "Actor/ServerPlayer.h"
#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Plugin/PluginEvent.h"
#include "Plugin/PluginManager.h"

#include <utility>

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

bool PluginPackets::_dispatch(PluginManager &manager, uint32_t type, const NetworkIdentifier &id,
                              std::string &data) {
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
