#include "Network/TransportFactory.h"

#include "Core/Debug/BedrockLog.h"
#include "Network/NetherNet/NetherNetInstance.h"
#include "Network/RakNetInstance.h"

bool TransportFactory::isSupported(TransportLayer layer) {
    return layer == TransportLayer::RakNet || layer == TransportLayer::NetherNet;
}

std::unique_ptr<Connector> TransportFactory::createConnector(TransportLayer layer,
                                                             RakPeerHelper::IPSupportInterface &ipSupport,
                                                             bool isServer) {
    if (layer == TransportLayer::RakNet)
        return std::unique_ptr<Connector>(new RakNetInstance(ipSupport, isServer));

    if (layer == TransportLayer::NetherNet)
        return std::unique_ptr<Connector>(new NetherNetInstance(ipSupport, isServer));

    LOG_ERROR(LogAreaID::Network, "No transport implementation available for %s", toString(layer));
    return nullptr;
}
