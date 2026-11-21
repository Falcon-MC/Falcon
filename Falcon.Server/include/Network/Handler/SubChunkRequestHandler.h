#pragma once

#include <cstdint>

class ServerNetworkHandler;
class ServerPlayer;
class SubChunkRequestPacket;

class SubChunkRequestHandler {
public:
    static const uint32_t MAX_ENTRIES_PER_RESPONSE = 256;

    static void handleRequest(ServerNetworkHandler &owner, ServerPlayer &player, const SubChunkRequestPacket &packet);
};
