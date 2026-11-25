#pragma once

#include "Network/NetworkIdentifier.h"
#include "Network/NetworkPeer.h"
#include "RakNet/RakPeerInterface.h"

#include <deque>
#include <mutex>

class RakNetRemotePeer : public NetworkPeer {
public:
    RakNetRemotePeer(RakNet::RakPeerInterface *rakPeer, const NetworkIdentifier &id);

    void sendPacket(const std::string &data, Reliability reliability, Compressibility compressibility) override;

    DataStatus receivePacket(std::string &outData) override;

    NetworkStatus getNetworkStatus() const override;

    uint16_t getMtuSize() const override;

    const NetworkIdentifier &getNetworkIdentifier() const { return mId; }

    void onDataReceived(const unsigned char *data, unsigned int length);

private:
    static RakNet::PacketReliability _toRakNetReliability(Reliability reliability);

    RakNet::RakPeerInterface *mRakPeer;
    NetworkIdentifier mId;

    mutable std::mutex mMutex;
    std::deque<std::string> mIncomingData;
};
