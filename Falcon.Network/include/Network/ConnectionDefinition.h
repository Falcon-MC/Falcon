#pragma once

#include <string>

struct ConnectionDefinition {
    static constexpr int MAX_TRANSPORT_CONNECTIONS = 1024;

    unsigned short mPort;
    unsigned short mPortV6;
    int mMaxNumPlayers;
    int mMaxNumConnections;
    std::string mIPv4Address;
    std::string mIPv6Address;
    bool mNeedsHostDiscovery;

    ConnectionDefinition()
            : mPort(19132), mPortV6(19133), mMaxNumPlayers(10), mMaxNumConnections(10),
              mIPv4Address("0.0.0.0"), mIPv6Address("::"), mNeedsHostDiscovery(true) {}

    static ConnectionDefinition createFromPorts(unsigned short port, unsigned short portV6, int maxNumPlayers) {
        ConnectionDefinition definition;
        definition.mPort = port;
        definition.mPortV6 = portV6;
        definition.mMaxNumPlayers = maxNumPlayers;
        definition.mMaxNumConnections = MAX_TRANSPORT_CONNECTIONS;
        return definition;
    }
};
