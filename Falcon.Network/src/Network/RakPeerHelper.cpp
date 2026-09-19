#include "Network/RakPeerHelper.h"

#include "RakNet/RakNetTypes.h"

RakPeerHelper::RakPeerHelper(IPSupportInterface *ipSupportInterface) : mIPSupportInterface(ipSupportInterface) {}

RakNet::StartupResult RakPeerHelper::peerStartup(RakNet::RakPeerInterface *rakPeer,
                                                 const ConnectionDefinition &definition, PeerPurpose purpose) {
    if (rakPeer == nullptr)
        return RakNet::INVALID_SOCKET_DESCRIPTORS;

    RakNet::SocketDescriptor socketDescriptor(definition.mPort, "::");
    socketDescriptor.socketFamily = AF_INET6;

    const unsigned int maxConnections = purpose == PeerPurpose::Server
                                        ? (unsigned int) definition.mMaxNumConnections
                                        : 1u;

    const RakNet::StartupResult result = rakPeer->Startup(maxConnections, &socketDescriptor, 1);

    if (result == RakNet::RAKNET_STARTED) {
        rakPeer->SetMaximumIncomingConnections(
                purpose == PeerPurpose::Server ? (unsigned short) definition.mMaxNumConnections : 0);

        if (mIPSupportInterface != nullptr)
            mIPSupportInterface->onReceiveIPSupport(IPSupport::IPv4);
    }

    return result;
}
