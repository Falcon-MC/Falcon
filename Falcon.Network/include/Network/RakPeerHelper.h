#pragma once

#include "Network/ConnectionDefinition.h"
#include "RakNet/RakPeerInterface.h"

class RakPeerHelper {
public:
    enum class PeerPurpose : int {
        Server = 0,
        Client = 1
    };

    enum class IPSupport : int {
        None = 0,
        IPv4 = 1,
        IPv6 = 2,
        Both = 3
    };

    class IPSupportInterface {
    public:
        virtual ~IPSupportInterface() = default;

        virtual void onReceiveIPSupport(IPSupport support) = 0;
    };

    explicit RakPeerHelper(IPSupportInterface *ipSupportInterface);

    RakNet::StartupResult peerStartup(RakNet::RakPeerInterface *rakPeer, const ConnectionDefinition &definition,
                                      PeerPurpose purpose);

private:
    IPSupportInterface *mIPSupportInterface;
};
