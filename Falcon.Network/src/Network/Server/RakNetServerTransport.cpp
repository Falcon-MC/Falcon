#include "Network/Server/RakNetServerTransport.h"

RakNetServerTransport::RakNetServerTransport(RakNet::RakPeerInterface *rakPeer, const RakNet::RakNetGUID &guid)
        : mRakPeer(rakPeer), mGuid(guid) {
}

void RakNetServerTransport::_closeTransport() {
    mRakPeer->CloseConnection(mGuid, true);
}
