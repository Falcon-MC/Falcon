#pragma once

#include <string>

struct ConnectionDefinition {
    static constexpr int MAX_TRANSPORT_CONNECTIONS = 1024;
    static constexpr const char *ANY_IPV4_ADDRESS = "0.0.0.0";

    unsigned short mPort;
    unsigned short mPortV6;
    int mMaxNumPlayers;
    int mMaxNumConnections;
    /**
     * The wildcard listens on every interface over both IP versions. Any other address binds an IPv4 socket
     * to that address alone, such as the loopback address for a server only this machine may join.
     */
    std::string mIPv4Address;
    std::string mIPv6Address;
    bool mNeedsHostDiscovery;

    ConnectionDefinition()
            : mPort(19132), mPortV6(19133), mMaxNumPlayers(10), mMaxNumConnections(10),
              mIPv4Address(ANY_IPV4_ADDRESS), mIPv6Address("::"), mNeedsHostDiscovery(true) {}

    static ConnectionDefinition createFromPorts(unsigned short port, unsigned short portV6, int maxNumPlayers) {
        ConnectionDefinition definition;
        definition.mPort = port;
        definition.mPortV6 = portV6;
        definition.mMaxNumPlayers = maxNumPlayers;
        definition.mMaxNumConnections = MAX_TRANSPORT_CONNECTIONS;
        return definition;
    }
};
