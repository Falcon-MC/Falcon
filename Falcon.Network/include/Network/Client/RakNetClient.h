#pragma once

#include "Network/Client/ClientTransport.h"
#include "Network/NetworkEnums.h"
#include "Network/NetworkIdentifier.h"
#include "Network/RakNetRemotePeer.h"
#include "Network/RakPeerHelper.h"
#include "RakNet/RakPeerInterface.h"

#include <atomic>
#include <memory>
#include <string>

class RakNetClient : public ClientTransport {
public:
    RakNetClient();

    ~RakNetClient() override;

    RakNetClient(const RakNetClient &) = delete;

    RakNetClient &operator=(const RakNetClient &) = delete;

    bool connect(const std::string &host, unsigned short port, unsigned int timeoutMs,
                 const std::atomic<bool> *cancel, std::string &outError);

    void runEvents() override;

    void close() override;

    bool isConnected() const override {
        return mConnected.load();
    }

    DisconnectFailReason getCloseReason() const override {
        return mCloseReason.load();
    }

    const std::shared_ptr<RakNetRemotePeer> &getPeer() const {
        return mPeer;
    }

    const NetworkIdentifier &getServerIdentifier() const {
        return mServerId;
    }

    const std::string &getResolvedAddress() const {
        return mResolvedAddress;
    }

private:
    static bool _resolve(const std::string &host, std::string &outAddress);

    void _markClosed(DisconnectFailReason reason);

    RakNet::RakPeerInterface *mRakPeer;
    RakPeerHelper mHelper;
    std::shared_ptr<RakNetRemotePeer> mPeer;
    NetworkIdentifier mServerId;
    std::string mResolvedAddress;
    bool mStarted;
    std::atomic<bool> mConnected;
    std::atomic<DisconnectFailReason> mCloseReason;
};
