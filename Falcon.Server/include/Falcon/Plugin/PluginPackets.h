#pragma once

#include <cstdint>
#include <string>
#include <vector>

class Level;
class NetworkIdentifier;
class Packet;
class ServerNetworkHandler;
class Vector3f;
class PluginManager;

class PluginPackets {
public:
    static uint32_t worldEffectType(const Packet &packet);
    static std::vector<NetworkIdentifier> broadcastWorldEffect(
            ServerNetworkHandler &owner, Level &level, const Vector3f &position, const Packet &packet, uint32_t type,
            std::vector<NetworkIdentifier> viewers);

    static std::string encode(uint32_t header, const std::string &payload);

    static bool onReceive(PluginManager &manager, const NetworkIdentifier &id, std::string &data);

    static bool onSend(PluginManager &manager, const NetworkIdentifier &id, std::string &data);

private:
    static bool _dispatch(PluginManager &manager, uint32_t type, const NetworkIdentifier &id, std::string &data);
};
