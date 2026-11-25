#include "Network/RakNetServerLocator.h"

RakNetServerLocator::RakNetServerLocator() : mRakPeer(nullptr) {}

void RakNetServerLocator::setRakPeer(RakNet::RakPeerInterface *rakPeer) {
    mRakPeer = rakPeer;
}

void RakNetServerLocator::announceServer(const PingedCompatibleServer &announcement, unsigned short portV4,
                                         unsigned short portV6) {
    if (mRakPeer == nullptr)
        return;

    PingedCompatibleServer server = announcement;
    server.mServerId = mRakPeer->GetMyGUID().g;

    const std::string rawAnnouncement = server.toAnnouncement(portV4, portV6);
    if (rawAnnouncement == mCurrentAnnouncement)
        return;

    mCurrentAnnouncement = rawAnnouncement;
    mRakPeer->SetOfflinePingResponse(mCurrentAnnouncement.c_str(), (unsigned int) mCurrentAnnouncement.length());
}

void RakNetServerLocator::stopAnnouncingServer() {
    if (mRakPeer == nullptr)
        return;

    mCurrentAnnouncement.clear();
    mRakPeer->SetOfflinePingResponse("", 0);
}
