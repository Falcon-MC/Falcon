#include "Network/RakNetInstance.h"

#include "RakNet/MessageIdentifiers.h"


RakNetInstance::RakNetInstance(RakPeerHelper::IPSupportInterface &ipSupport, bool isServer)
        : mRakPeer(RakNet::RakPeerInterface::GetInstance()), mHelper(&ipSupport), mIsServer(isServer),
          mIsHosting(false) {
    mServerLocator.setRakPeer(mRakPeer);
}

RakNetInstance::~RakNetInstance() {
    disconnect();
    RakNet::RakPeerInterface::DestroyInstance(mRakPeer);
    mRakPeer = nullptr;
}

bool RakNetInstance::host(const ConnectionDefinition &definition) {
    if (mIsHosting)
        return false;

    mConnectionDefinition = definition;

    const RakNet::StartupResult result = mHelper.peerStartup(mRakPeer, definition,
                                                             mIsServer ? RakPeerHelper::PeerPurpose::Server
                                                                       : RakPeerHelper::PeerPurpose::Client);
    if (result != RakNet::RAKNET_STARTED)
        return false;

    mIsHosting = true;
    return true;
}

void RakNetInstance::disconnect() {
    if (!mIsHosting)
        return;

    mServerLocator.stopAnnouncingServer();
    mRakPeer->Shutdown(300);
    mRemotePeers.clear();
    mIsHosting = false;
}

bool RakNetInstance::isIPv4Supported() const {
    return true;
}

bool RakNetInstance::isIPv6Supported() const {
    return false;
}

unsigned short RakNetInstance::getConnectedPeerCount() const {
    return (unsigned short) mRemotePeers.size();
}

unsigned short RakNetInstance::getBoundPort() const {
    return mIsHosting ? mRakPeer->GetMyBoundAddress().GetPort() : 0;
}

void RakNetInstance::announceServer(const PingedCompatibleServer &announcement) {
    mServerLocator.announceServer(announcement, mConnectionDefinition.mPort, mConnectionDefinition.mPortV6);
}

NetworkIdentifier RakNetInstance::_createNetworkIdentifier(const RakNet::Packet *packet) const {
    return NetworkIdentifier(packet->guid, packet->systemAddress);
}

std::shared_ptr<NetworkPeer> RakNetInstance::getPeerForUser(const NetworkIdentifier &id) {
    auto it = mRemotePeers.find(id);
    return it == mRemotePeers.end() ? nullptr : it->second;
}

void RakNetInstance::_onNewIncomingConnection(const RakNet::Packet *packet) {
    const NetworkIdentifier id = _createNetworkIdentifier(packet);

    if (mRemotePeers.find(id) != mRemotePeers.end())
        return;

    if (mCallbacks != nullptr && !mCallbacks->onValidateIncomingConnection(id)) {
        mRakPeer->CloseConnection(id.getGuid(), true);
        return;
    }

    std::shared_ptr<RakNetRemotePeer> peer = std::make_shared<RakNetRemotePeer>(mRakPeer, id);
    mRemotePeers[id] = peer;

    if (mCallbacks != nullptr)
        mCallbacks->onNewIncomingConnection(id, peer);
}

bool RakNetInstance::closeConnection(const NetworkIdentifier &id) {
    if (mRemotePeers.erase(id) == 0)
        return false;

    mRakPeer->CloseConnection(id.getGuid(), false);
    return true;
}

void RakNetInstance::_onConnectionClosed(const RakNet::Packet *packet, DisconnectFailReason reason) {
    const NetworkIdentifier id = _createNetworkIdentifier(packet);

    if (mRemotePeers.erase(id) == 0)
        return;

    if (mCallbacks != nullptr)
        mCallbacks->onConnectionClosed(id, reason, toString(reason));
}

void RakNetInstance::_onDataReceived(const RakNet::Packet *packet) {
    auto it = mRemotePeers.find(_createNetworkIdentifier(packet));
    if (it == mRemotePeers.end())
        return;

    it->second->onDataReceived(packet->data, packet->length);
}

void RakNetInstance::runEvents() {
    if (!mIsHosting)
        return;

    for (;;) {
        RakNet::Packet *packet = mRakPeer->Receive();
        if (packet == nullptr)
            break;

        switch (packet->data[0]) {
            case RakNet::ID_NEW_INCOMING_CONNECTION:
                _onNewIncomingConnection(packet);
                break;

            case RakNet::ID_DISCONNECTION_NOTIFICATION:
                _onConnectionClosed(packet, DisconnectFailReason::Disconnected);
                break;

            case RakNet::ID_CONNECTION_LOST:
                _onConnectionClosed(packet, DisconnectFailReason::Timeout);
                break;

            default:
                _onDataReceived(packet);
                break;
        }

        mRakPeer->DeallocatePacket(packet);
    }
}
