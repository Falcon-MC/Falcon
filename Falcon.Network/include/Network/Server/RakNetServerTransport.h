#pragma once

#include "Network/Server/ServerTransport.h"
#include "RakNet/RakPeerInterface.h"

class RakNetServerTransport : public ServerTransport {
public:
    RakNetServerTransport(RakNet::RakPeerInterface *rakPeer, const RakNet::RakNetGUID &guid);

protected:
    void _closeTransport() override;

private:
    RakNet::RakPeerInterface *mRakPeer;
    RakNet::RakNetGUID mGuid;
};
