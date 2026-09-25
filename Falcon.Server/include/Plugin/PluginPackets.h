#pragma once

#include <cstdint>
#include <string>

class NetworkIdentifier;
class PluginManager;

class PluginPackets {
public:
    static std::string encode(uint32_t header, const std::string &payload);

    static bool onReceive(PluginManager &manager, const NetworkIdentifier &id, std::string &data);

    static bool onSend(PluginManager &manager, const NetworkIdentifier &id, std::string &data);

private:
    static bool _dispatch(PluginManager &manager, uint32_t type, const NetworkIdentifier &id, std::string &data);
};
