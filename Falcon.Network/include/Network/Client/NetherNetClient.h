#pragma once

#include "Network/Client/ClientTransport.h"
#include "Network/NetherNet/NetherNetConnection.h"
#include "Network/NetherNet/NetherNetSignaling.h"
#include "Network/NetworkEnums.h"
#include "Network/NetworkIdentifier.h"

#include <atomic>
#include <memory>
#include <string>

class KeyPair;

struct NetherNetDialOptions {
    std::shared_ptr<KeyPair> mIdentityKey;
    std::string mIdentityToken;
    std::string mIdentityDomain;
    bool mAllowIdentitylessServer = false;
    bool mDisableTrickleIce = false;
    bool mCloseSignalingOnClose = false;
    unsigned long long mConnectionID = 0;
};

class NetherNetClient : public ClientTransport {
public:
    NetherNetClient();

    ~NetherNetClient() override;

    NetherNetClient(const NetherNetClient &) = delete;

    NetherNetClient &operator=(const NetherNetClient &) = delete;

    bool connect(const std::string &networkID, const std::shared_ptr<nethernet::Signaling> &signaling,
                 const NetherNetDialOptions &options, unsigned int timeoutMs, const std::atomic<bool> *cancel,
                 std::string &outError);

    void runEvents() override;

    void close() override;

    bool isConnected() const override;

    DisconnectFailReason getCloseReason() const override;

    const std::shared_ptr<nethernet::Connection> &getPeer() const;

    const NetworkIdentifier &getServerIdentifier() const;

    const std::shared_ptr<nethernet::Signaling> &getSignaling() const;

private:
    struct State;

    bool _negotiate(const std::string &networkID, unsigned int timeoutMs, const std::atomic<bool> *cancel,
                    std::string &outError);

    bool _createOffer(unsigned int timeoutMs, std::string &outOffer, std::string &outError);

    bool _acceptAnswer(const std::string &answer, std::string &outError, int &outErrorCode);

    bool _addRemoteCandidate(const std::string &data, std::string &outError);

    void _flushLocalCandidates();

    void _signalError(int code);

    void _teardown();

    void _markClosed(DisconnectFailReason reason);

    std::shared_ptr<State> mState;
    std::shared_ptr<nethernet::Signaling> mSignaling;
    std::shared_ptr<rtc::PeerConnection> mPeerConnection;
    std::shared_ptr<nethernet::Connection> mPeer;
    NetherNetDialOptions mOptions;
    NetworkIdentifier mServerId;
    std::string mRemoteNetworkID;
    std::string mUsernameFragment;
    unsigned long long mConnectionID;
    unsigned int mSubscription;
    unsigned int mNextCandidateIndex;
    bool mSubscribed;
    bool mTrickleIce;
    bool mOfferSent;
    bool mStarted;
    std::atomic<bool> mConnected;
    std::atomic<DisconnectFailReason> mCloseReason;
};
