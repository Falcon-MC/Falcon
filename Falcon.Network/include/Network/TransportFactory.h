#pragma once

#include "Network/Connector.h"
#include "Network/NetworkEnums.h"
#include "Network/RakPeerHelper.h"

#include <memory>

class TransportFactory {
public:
    static bool isSupported(TransportLayer layer);

    static std::unique_ptr<Connector> createConnector(TransportLayer layer,
                                                      RakPeerHelper::IPSupportInterface &ipSupport,
                                                      bool isServer);
};
