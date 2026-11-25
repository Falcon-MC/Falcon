#include "Network/RakNetRemotePeer.h"

RakNetRemotePeer::RakNetRemotePeer(RakNet::RakPeerInterface *rakPeer, const NetworkIdentifier &id)
        : mRakPeer(rakPeer), mId(id) {}

RakNet::PacketReliability RakNetRemotePeer::_toRakNetReliability(Reliability reliability) {
    switch (reliability) {
        case Reliability::Reliable:
            return RakNet::RELIABLE;
        case Reliability::Unreliable:
            return RakNet::UNRELIABLE;
        case Reliability::UnreliableSequenced:
            return RakNet::UNRELIABLE_SEQUENCED;
        default:
            return RakNet::RELIABLE_ORDERED;
    }
}

void RakNetRemotePeer::sendPacket(const std::string &data, Reliability reliability, Compressibility compressibility) {
    (void) compressibility;

    if (data.empty() || mRakPeer == nullptr)
        return;

    mRakPeer->Send(data.data(), (int) data.size(), RakNet::MEDIUM_PRIORITY, _toRakNetReliability(reliability), 0,
                   mId.getGuid(), false);
}

NetworkPeer::DataStatus RakNetRemotePeer::receivePacket(std::string &outData) {
    std::lock_guard<std::mutex> guard(mMutex);

    if (mIncomingData.empty())
        return DataStatus::NoData;

    outData = std::move(mIncomingData.front());
    mIncomingData.pop_front();
    return DataStatus::HasData;
}

uint16_t RakNetRemotePeer::getMtuSize() const {
    if (mRakPeer == nullptr)
        return 0;

    const int mtu = mRakPeer->GetMTUSize(mId.getGuid());
    return mtu > 0 ? (uint16_t) mtu : 0;
}

NetworkPeer::NetworkStatus RakNetRemotePeer::getNetworkStatus() const {
    NetworkStatus status;

    if (mRakPeer != nullptr) {
        const int ping = mRakPeer->GetAveragePing(mId.getGuid());
        if (ping >= 0) {
            status.mCurrentPing = ping;
            status.mAveragePing = ping;
        }
    }

    return status;
}

void RakNetRemotePeer::onDataReceived(const unsigned char *data, unsigned int length) {
    std::lock_guard<std::mutex> guard(mMutex);
    mIncomingData.emplace_back((const char *) data, length);
}
