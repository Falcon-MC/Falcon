#pragma once

#include "Network/ConnectionDefinition.h"
#include "Network/NetworkEnums.h"
#include "Network/NetworkIdentifier.h"
#include "Network/NetworkPeer.h"

#include <memory>
#include <string>

class Connector {
public:
    class ConnectionCallbacks {
    public:
        virtual ~ConnectionCallbacks() = default;

        virtual bool onValidateIncomingConnection(const NetworkIdentifier &id) = 0;

        virtual void onNewIncomingConnection(const NetworkIdentifier &id, std::shared_ptr<NetworkPeer> peer) = 0;

        virtual void onConnectionClosed(const NetworkIdentifier &id, DisconnectFailReason reason,
                                        const std::string &message) = 0;
    };

    virtual ~Connector() = default;

    virtual bool host(const ConnectionDefinition &definition) = 0;

    virtual void disconnect() = 0;

    virtual void runEvents() = 0;

    virtual std::shared_ptr<NetworkPeer> getPeerForUser(const NetworkIdentifier &id) = 0;

    /**
     * Drops one connection without notifying the remote system. Returns false when this connector does not
     * own it.
     */
    virtual bool closeConnection(const NetworkIdentifier &) { return false; }

    virtual TransportLayer getNetworkType() const = 0;

    virtual bool isIPv4Supported() const = 0;

    virtual bool isIPv6Supported() const = 0;

    virtual void setCallbacks(ConnectionCallbacks *callbacks) { mCallbacks = callbacks; }

protected:
    ConnectionCallbacks *mCallbacks = nullptr;
};
