#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Plugin/PluginApiHelpers.h"
#include "Plugin/PluginPackets.h"
#include "Plugin/PluginServerApi.h"

#include <string>

using namespace PluginApiHelpers;

namespace {
    int playerSendPacket(FalconPlayer *target, uint32_t packetId, const uint8_t *data, uint32_t length) {
        if (target == nullptr || packetId > 0x3ff || (data == nullptr && length != 0))
            return 0;

        ServerNetworkHandler &handler = owner();
        if (!handler.isTransportReady())
            return 0;

        const std::string payload = data == nullptr ? std::string()
                                                    : std::string(reinterpret_cast<const char *>(data), length);
        handler.getNetworkHandler().send(player(target)->getNetworkIdentifier(),
                                         PluginPackets::encode(packetId, payload));
        return 1;
    }
}

void PluginServerApi::fillPackets(FalconServerApi &api) {
    api.playerSendPacket = &playerSendPacket;
}
