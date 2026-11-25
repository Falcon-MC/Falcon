#pragma once

#include "Network/Connector.h"
#include "Network/RakNetRemotePeer.h"
#include "Network/RakNetServerLocator.h"
#include "Network/RakPeerHelper.h"

#include <memory>
#include <unordered_map>

class RakNetInstance : public Connector {
public:
    RakNetInstance(RakPeerHelper::IPSupportInterface &ipSupport, bool isServer);

    ~RakNetInstance() override;

    bool host(const ConnectionDefinition &definition) override;

    void disconnect() override;

    void runEvents() override;

    std::shared_ptr<NetworkPeer> getPeerForUser(const NetworkIdentifier &id) override;

    bool closeConnection(const NetworkIdentifier &id) override;

    TransportLayer getNetworkType() const override { return TransportLayer::RakNet; }

    bool isIPv4Supported() const override;

    bool isIPv6Supported() const override;

    void announceServer(const PingedCompatibleServer &announcement);

    RakNetServerLocator &getServerLocator() { return mServerLocator; }

    RakNet::RakPeerInterface *getPeer() const { return mRakPeer; }

    const ConnectionDefinition &getConnectionDefinition() const { return mConnectionDefinition; }

    unsigned short getConnectedPeerCount() const;

    /**
     * The port the socket really listens on, which the system picks when the definition asked for port 0.
     */
    unsigned short getBoundPort() const;

private:
    void _onNewIncomingConnection(const RakNet::Packet *packet);

    void _onConnectionClosed(const RakNet::Packet *packet, DisconnectFailReason reason);

    void _onDataReceived(const RakNet::Packet *packet);

    NetworkIdentifier _createNetworkIdentifier(const RakNet::Packet *packet) const;

    RakNet::RakPeerInterface *mRakPeer;
    RakPeerHelper mHelper;
    RakNetServerLocator mServerLocator;
    ConnectionDefinition mConnectionDefinition;

    std::unordered_map<NetworkIdentifier, std::shared_ptr<RakNetRemotePeer>, NetworkIdentifier::Hasher> mRemotePeers;

    bool mIsServer;
    bool mIsHosting;
};
