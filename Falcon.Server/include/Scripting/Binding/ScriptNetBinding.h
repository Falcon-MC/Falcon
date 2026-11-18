#pragma once

#include "Protocol/MinecraftPacketIds.h"

#include <string>
#include <vector>

#include <quickjs.h>

class ScriptApi;
struct ScriptNetModuleConfig;

// @minecraft/server-net: http requests and web sockets on worker threads, packet before events
namespace ScriptNetBinding {
    // the module's exports, then hooks for the engine
    JSValue install(ScriptApi &api, JSValueConst secretOf);

    const std::vector<const char *> &exportNames();

    // the finished requests and socket events of one pack, delivered on the game thread
    void tick(ScriptApi &api);

    // the PacketId member naming a packet, nullptr for a packet the enum leaves out
    const char *packetName(MinecraftPacketIds id);

    enum class UriCheck { Allowed, TlsOnly, BadScheme, NoHost, NotAllowed };

    // the pack's allowed_uris and force_tls; webSocket picks wss/ws over https/http
    UriCheck checkUri(const ScriptNetModuleConfig &config, const std::string &text, bool webSocket);
}
