#pragma once

#include "Network/PingedCompatibleServer.h"
#include "RakNet/RakPeerInterface.h"

#include <string>

class RakNetServerLocator {
public:
    RakNetServerLocator();

    void setRakPeer(RakNet::RakPeerInterface *rakPeer);

    void announceServer(const PingedCompatibleServer &announcement, unsigned short portV4, unsigned short portV6);

    void stopAnnouncingServer();

    const std::string &getCurrentAnnouncement() const { return mCurrentAnnouncement; }

private:
    RakNet::RakPeerInterface *mRakPeer;
    std::string mCurrentAnnouncement;
};
