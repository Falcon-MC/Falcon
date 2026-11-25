#pragma once

#include "Network/Client/ClientNetworkSystem.h"
#include "Network/NetworkEnums.h"

#include <string>

struct SessionConnectionTarget {
    TransportLayer mTransportLayer = TransportLayer::Unknown;
    NetherNetSignalingType mSignalingType = NetherNetSignalingType::WebSocket;
    std::string mNetworkId;
    std::string mHost;
    unsigned short mPort = 0;
    std::string mNonce;

    void applyTo(ClientConnectionSettings &settings) const;
};
