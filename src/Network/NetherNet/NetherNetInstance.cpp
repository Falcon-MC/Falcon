#include "Network/NetherNet/NetherNetInstance.h"

#include "Core/Debug/BedrockLog.h"
#include "Network/NetherNet/NetherNetDescription.h"
#include "Network/NetherNet/NetherNetSignal.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <chrono>
#include <condition_variable>
#include <random>

namespace {

    // a client that never reads its disconnect keeps the connection this long at most
    const std::chrono::milliseconds CLOSE_GRACE(1000);
    const int GATHERING_TIMEOUT_SECONDS = 15;
    const unsigned int SIGNAL_TIMEOUT_MS = 10000;
    const char *IDENTITY_DOMAIN = "self";

    unsigned long long randomNetworkID() {
        std::random_device device;
        std::mt19937_64 engine(device());
        return engine();
    }

    const char *setupRoleName(rtc::Description::Role role) {
        switch (role) {
            case rtc::Description::Role::Active:
                return "active";
            case rtc::Description::Role::Passive:
                return "passive";
            default:
                return "actpass";
        }
    }

}

NetherNetInstance::NetherNetInstance(RakPeerHelper::IPSupportInterface &ipSupport, bool isServer)
        : mIpSupport(ipSupport), mNextConnectionID(randomNetworkID()), mIsServer(isServer), mIsHosting(false) {
}

NetherNetInstance::~NetherNetInstance() {
    disconnect();
}

bool NetherNetInstance::isIPv4Supported() const {
    return true;
}

bool NetherNetInstance::isIPv6Supported() const {
    return false;
}

void NetherNetInstance::setCredentials(const nethernet::Credentials &credentials) {
    std::lock_guard<std::mutex> lock(mMutex);
    mCredentials = credentials;
}

void NetherNetInstance::setTlsCertificate(const std::string &certificatePath, const std::string &privateKeyPath) {
    mSignaling.setCertificate(certificatePath, privateKeyPath);
    mExtraSignaling.setCertificate(certificatePath, privateKeyPath);
}

bool NetherNetInstance::host(const ConnectionDefinition &definition) {
    if (mIsHosting)
        return false;

    if (!mIsServer) {
        LOG_ERROR(LogAreaID::Network, "NetherNet client mode is not implemented");
        return false;
    }

    mConnectionDefinition = definition;

    if (!mIdentity.generate(IDENTITY_DOMAIN)) {
        LOG_ERROR(LogAreaID::Network, "Failed to generate the NetherNet server identity key");
        return false;
    }

    const nethernet::SignalingServer::OfferHandler handler =
            [this](const std::string &networkID, const std::string &offer, std::string &answer, int &errorCode) {
                return _negotiate(networkID, offer, answer, errorCode);
            };

    const nethernet::SignalingServer::StatusProvider status = [this]() {
        const nethernet::ServerData data = mServerDataProvider ? mServerDataProvider() : nethernet::ServerData();
        return data.toStatusJson();
    };
    mSignaling.setStatusProvider(status);

    if (mAcmeEnabled) {
        mAcmeChallenges = std::make_shared<acme::ChallengeStore>();
        mSignaling.setChallengeStore(mAcmeChallenges);
        mExtraSignaling.setChallengeStore(mAcmeChallenges);
    }

    if (!mSignaling.start(definition.mIPv4Address, definition.mPort, handler))
        return false;

    // the game probes https://host and http://host beside the game port only when that port is 19132:
    // a refused extra port leaves the game port serving alone
    if (mExtraSignalingPort != 0 && mExtraSignalingPort != mSignaling.getBoundPort()) {
        mExtraSignaling.setStatusProvider(status);
        if (!mExtraSignaling.start(definition.mIPv4Address, mExtraSignalingPort, handler))
            LOG_WARN(LogAreaID::Network, "NetherNet signaling could not also listen on TCP %u",
                     (unsigned) mExtraSignalingPort);
    }

    if (mAcmeEnabled) {
        mAcme.reset(new acme::Manager(mAcmeSettings, mAcmeChallenges,
                                      [this](const std::string &certificate, const std::string &key) {
                                          const bool installed = mSignaling.reloadCertificate(certificate, key);
                                          mExtraSignaling.reloadCertificate(certificate, key);
                                          return installed;
                                      }));
        if (!mAcme->start())
            mAcme.reset();
    }

    unsigned char rawId[8];
    RAND_bytes(rawId, sizeof(rawId));
    mNetworkId = 0;
    for (size_t i = 0; i < sizeof(rawId); i++)
        mNetworkId |= ((uint64_t) rawId[i]) << (i * 8);

    const nethernet::DiscoveryListener::ServerDataProvider dataProvider =
            [this]() {
                if (mServerDataProvider)
                    return mServerDataProvider();

                return nethernet::ServerData();
            };

    const nethernet::DiscoveryListener::OfferHandler lanHandler =
            [this](const std::string &networkID, const std::string &offer, std::string &answer, int &errorCode,
                   bool localSender) {
                return _negotiate(networkID, offer, answer, errorCode, std::string(),
                                  localSender && mAnonymousLanClientsAllowed);
            };

    mDiscovery.start(mNetworkId, dataProvider, lanHandler);

    mIsHosting = true;
    return true;
}

void NetherNetInstance::disconnect() {
    if (!mIsHosting)
        return;

    mIsHosting = false;
    _detachSignaling();
    mDiscovery.stop();
    if (mAcme != nullptr) {
        mAcme->stop();
        mAcme.reset();
    }
    mExtraSignaling.stop();
    mSignaling.stop();

    std::vector<std::shared_ptr<nethernet::Connection>> connections;

    {
        std::lock_guard<std::mutex> lock(mMutex);

        for (const std::shared_ptr<nethernet::Connection> &connection: mPending)
            connections.push_back(connection);

        for (const auto &entry: mPeers)
            connections.push_back(entry.second);

        for (const ClosingPeer &closing: mClosing)
            connections.push_back(closing.mConnection);

        mPending.clear();
        mPeers.clear();
        mClosing.clear();
    }

    for (const std::shared_ptr<nethernet::Connection> &connection: connections)
        connection->close();

    mIsHosting = false;
}

bool NetherNetInstance::closeConnection(const NetworkIdentifier &id) {
    std::shared_ptr<nethernet::Connection> connection;

    {
        std::lock_guard<std::mutex> lock(mMutex);

        const auto it = mPeers.find(id);
        if (it == mPeers.end())
            return false;

        connection = it->second;
        mPeers.erase(it);
        mClosing.push_back(ClosingPeer{connection, std::chrono::steady_clock::now() + CLOSE_GRACE});
    }

    connection->stopReceiving();
    return true;
}

std::shared_ptr<NetworkPeer> NetherNetInstance::getPeerForUser(const NetworkIdentifier &id) {
    std::lock_guard<std::mutex> lock(mMutex);

    const auto it = mPeers.find(id);
    return it == mPeers.end() ? nullptr : it->second;
}

std::string NetherNetInstance::_signalKey(const std::string &networkID, uint64_t connectionID) {
    return networkID + ":" + std::to_string(connectionID);
}

void NetherNetInstance::attachSignaling(const std::shared_ptr<nethernet::Signaling> &signaling) {
    _detachSignaling();
    if (signaling == nullptr)
        return;

    nethernet::Credentials credentials;
    std::string error;
    if (signaling->requestCredentials(credentials, SIGNAL_TIMEOUT_MS, error))
        setCredentials(credentials);
    else
        LOG_WARN(LogAreaID::Network, "Could not fetch the NetherNet relay credentials: %s", error.c_str());

    std::lock_guard<std::mutex> lock(mMutex);
    mOnlineSignaling = signaling;
    mSignalSubscription = signaling->subscribe([this](const nethernet::Signal &signal) {
        _onSignal(signal);
    });
}

void NetherNetInstance::_detachSignaling() {
    std::shared_ptr<nethernet::Signaling> signaling;
    std::vector<std::thread> threads;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        signaling = std::move(mOnlineSignaling);
        mOnlineSignaling = nullptr;
        threads.swap(mNegotiationThreads);
    }

    if (signaling != nullptr) {
        signaling->unsubscribe(mSignalSubscription);
        signaling->close();
    }

    for (std::thread &thread: threads) {
        if (thread.joinable())
            thread.join();
    }

    std::lock_guard<std::mutex> lock(mMutex);
    mNegotiating.clear();
    mNegotiatingConnections.clear();
    mBufferedCandidates.clear();
}

void NetherNetInstance::_onSignal(const nethernet::Signal &signal) {
    if (signal.mType == nethernet::SIGNAL_TYPE_CANDIDATE) {
        _addRemoteCandidate(_signalKey(signal.mNetworkID, signal.mConnectionID), signal.mData);
        return;
    }

    if (signal.mType != nethernet::SIGNAL_TYPE_OFFER || !mIsHosting)
        return;

    std::lock_guard<std::mutex> lock(mMutex);
    mNegotiationThreads.emplace_back(&NetherNetInstance::_answerOffer, this, signal);
}

void NetherNetInstance::_answerOffer(nethernet::Signal offer) {
    std::shared_ptr<nethernet::Signaling> signaling;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        signaling = mOnlineSignaling;
    }

    if (signaling == nullptr)
        return;

    const std::string key = _signalKey(offer.mNetworkID, offer.mConnectionID);

    std::string answer;
    int errorCode = 0;
    const bool accepted = _negotiate(offer.mNetworkID, offer.mData, answer, errorCode, key);

    nethernet::Signal response;
    response.mConnectionID = offer.mConnectionID;
    response.mNetworkID = offer.mNetworkID;

    if (accepted) {
        response.mType = nethernet::SIGNAL_TYPE_ANSWER;
        response.mData = answer;
    } else {
        response.mType = nethernet::SIGNAL_TYPE_ERROR;
        response.mData = std::to_string(errorCode != 0 ? errorCode : (int) nethernet::ErrorCodeGenericFailure);
    }

    std::string error;
    if (!signaling->signal(response, SIGNAL_TIMEOUT_MS, error))
        LOG_WARN(LogAreaID::Network, "Could not answer the NetherNet offer from %s: %s", offer.mNetworkID.c_str(),
                 error.c_str());
}

void NetherNetInstance::_addRemoteCandidate(const std::string &signalKey, const std::string &candidate) {
    std::shared_ptr<rtc::PeerConnection> peerConnection;
    std::shared_ptr<nethernet::Connection> connection;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        const auto it = mNegotiating.find(signalKey);
        peerConnection = it != mNegotiating.end() ? it->second.lock() : nullptr;

        if (peerConnection == nullptr) {
            mBufferedCandidates[signalKey].push_back(candidate);
            return;
        }

        const auto owner = mNegotiatingConnections.find(signalKey);
        connection = owner != mNegotiatingConnections.end() ? owner->second.lock() : nullptr;
    }

    if (connection != nullptr)
        connection->addRemoteCandidate(candidate);

    try {
        peerConnection->addRemoteCandidate(rtc::Candidate(nethernet::Description::stripCandidatePrefix(candidate), "0"));
    } catch (const std::exception &error) {
        LOG_WARN(LogAreaID::Network, "Could not add a remote NetherNet candidate: %s", error.what());
    }
}

bool NetherNetInstance::_negotiate(const std::string &networkID, const std::string &offer, std::string &answer,
                                   int &errorCode, const std::string &signalKey, bool anonymousAllowed) {
    int clientCheck = 0;
    const bool accepted = _negotiateOffer(networkID, offer, answer, errorCode, signalKey, clientCheck,
                                          anonymousAllowed);
    if (!accepted && mNegotiationFailureObserver)
        mNegotiationFailureObserver(errorCode, clientCheck);
    return accepted;
}

bool NetherNetInstance::_negotiateOffer(const std::string &networkID, const std::string &offer, std::string &answer,
                                        int &errorCode, const std::string &signalKey, int &clientCheck,
                                        bool anonymousAllowed) {
    const int offerCheck = mUnofficialClientsAllowed ? 0 : nethernet::Description::inspectOffer(offer);
    if (offerCheck != 0) {
        LOG_WARN(LogAreaID::Network, "NetherNet offer from %s rejected: client check %d", networkID.c_str(),
                 offerCheck);
        errorCode = nethernet::ErrorCodeFailedToSetRemoteDescription;
        clientCheck = offerCheck;
        return false;
    }

    const std::vector<nethernet::Fingerprint> remoteFingerprints = nethernet::Description::parseFingerprints(offer);

    if (remoteFingerprints.empty()) {
        LOG_WARN(LogAreaID::Network, "NetherNet offer has no a=fingerprint line, rejecting identity assertion");
        errorCode = nethernet::ErrorCodeFailedToSetRemoteDescription;
        return false;
    }

    nethernet::IdentityData identity;
    std::string failure;

    const bool asserted = nethernet::Description::parseIdentity(offer, identity, failure);
    if (!asserted) {
        if (!failure.empty()) {
            LOG_WARN(LogAreaID::Network, "NetherNet identity assertion rejected: %s", failure.c_str());
            errorCode = nethernet::ErrorCodeIdentityVerificationFailed;
            return false;
        }

        if (!anonymousAllowed) {
            LOG_WARN(LogAreaID::Network, "NetherNet offer carries no identity assertion, rejecting anonymous client");
            errorCode = nethernet::ErrorCodeIdentityVerificationFailed;
            return false;
        }
    } else if (!identity.isValid()) {
        LOG_WARN(LogAreaID::Network, "NetherNet identity assertion is malformed");
        errorCode = nethernet::ErrorCodeIdentityVerificationFailed;
        return false;
    }

    // an assertion, when one is sent, is always checked
    if (asserted) {
        EVP_PKEY *publicKey = nethernet::Identity::claimPublicKey(identity.mAssertionToken, failure);
        if (publicKey == nullptr) {
            LOG_WARN(LogAreaID::Network, "NetherNet client token rejected: %s", failure.c_str());
            errorCode = nethernet::ErrorCodeIdentityVerificationFailed;
            return false;
        }

        const std::string remotePayload = nethernet::Description::generateFingerprintsPayload(remoteFingerprints);
        const bool verified = nethernet::Identity::verifyDetached(publicKey, identity.mAssertionFingerprints,
                                                                  remotePayload);
        EVP_PKEY_free(publicKey);

        if (!verified) {
            LOG_WARN(LogAreaID::Network, "NetherNet fingerprints assertion could not be verified");
            errorCode = nethernet::ErrorCodeIdentityVerificationFailed;
            return false;
        }
    }

    rtc::Configuration configuration;
    configuration.mtu = nethernet::NETWORK_MTU;
    configuration.maxMessageSize = nethernet::MAX_MESSAGE_SIZE + 1;

    {
        std::lock_guard<std::mutex> lock(mMutex);

        for (const nethernet::IceServer &server: mCredentials.mIceServers) {
            for (const std::string &url: server.mUrls) {
                rtc::IceServer entry(url);
                entry.username = server.mUsername;
                entry.password = server.mPassword;
                configuration.iceServers.push_back(entry);
            }
        }
    }

    unsigned long long connectionID;

    {
        std::lock_guard<std::mutex> lock(mMutex);
        connectionID = mNextConnectionID++;
    }

    std::shared_ptr<rtc::PeerConnection> peerConnection;

    try {
        peerConnection = std::make_shared<rtc::PeerConnection>(configuration);
    } catch (const std::exception &error) {
        LOG_ERROR(LogAreaID::Network, "Failed to create a NetherNet peer connection: %s", error.what());
        errorCode = nethernet::ErrorCodeFailedToCreatePeerConnection;
        return false;
    }

    const NetworkIdentifier id(networkID, connectionID);
    const std::shared_ptr<nethernet::Connection> connection =
            std::make_shared<nethernet::Connection>(id, peerConnection);

    connection->setRemoteFingerprint(remoteFingerprints.front().mValue);
    connection->setRemoteOffer(offer);
    connection->setRemoteRequest(nethernet::SignalingServer::currentRequestHeaders());

    std::weak_ptr<nethernet::Connection> weak = connection;

    peerConnection->onDataChannel([weak](std::shared_ptr<rtc::DataChannel> channel) {
        const std::shared_ptr<nethernet::Connection> target = weak.lock();
        if (target == nullptr) {
            channel->close();
            return;
        }

        if (!target->attachChannel(channel)) {
            LOG_WARN(LogAreaID::Network, "Rejecting unexpected NetherNet data channel %s", channel->label().c_str());
            target->close();
        }
    });

    std::shared_ptr<std::mutex> gatherMutex = std::make_shared<std::mutex>();
    std::shared_ptr<std::condition_variable> gatherSignal = std::make_shared<std::condition_variable>();
    std::shared_ptr<bool> gathered = std::make_shared<bool>(false);

    peerConnection->onGatheringStateChange([gatherMutex, gatherSignal, gathered](
            rtc::PeerConnection::GatheringState state) {
        if (state != rtc::PeerConnection::GatheringState::Complete)
            return;

        {
            std::lock_guard<std::mutex> lock(*gatherMutex);
            *gathered = true;
        }

        gatherSignal->notify_all();
    });

    try {
        peerConnection->setRemoteDescription(rtc::Description(offer, rtc::Description::Type::Offer));
    } catch (const std::exception &error) {
        LOG_WARN(LogAreaID::Network, "Failed to apply the NetherNet offer: %s", error.what());
        connection->close();
        errorCode = nethernet::ErrorCodeFailedToSetRemoteDescription;
        return false;
    }

    if (!signalKey.empty()) {
        std::vector<std::string> buffered;

        {
            std::lock_guard<std::mutex> lock(mMutex);
            mNegotiating[signalKey] = peerConnection;
            mNegotiatingConnections[signalKey] = connection;
            const auto it = mBufferedCandidates.find(signalKey);
            if (it != mBufferedCandidates.end()) {
                buffered = std::move(it->second);
                mBufferedCandidates.erase(it);
            }
        }

        for (const std::string &candidate: buffered)
            _addRemoteCandidate(signalKey, candidate);
    }

    {
        std::unique_lock<std::mutex> lock(*gatherMutex);

        if (peerConnection->gatheringState() == rtc::PeerConnection::GatheringState::Complete)
            *gathered = true;

        if (!*gathered) {
            gatherSignal->wait_for(lock, std::chrono::seconds(GATHERING_TIMEOUT_SECONDS),
                                   [gathered]() { return *gathered; });
        }

        if (!*gathered) {
            lock.unlock();
            LOG_WARN(LogAreaID::Network, "Timed out gathering local NetherNet candidates");
            connection->close();
            errorCode = nethernet::ErrorCodeNegotiationTimeoutWaitingForResponse;
            return false;
        }
    }

    const std::optional<rtc::Description> local = peerConnection->localDescription();
    if (!local.has_value()) {
        LOG_WARN(LogAreaID::Network, "NetherNet peer connection produced no local description");
        connection->close();
        errorCode = nethernet::ErrorCodeFailedToCreateAnswer;
        return false;
    }

    const rtc::Description &localDescription = local.value();
    const std::optional<rtc::CertificateFingerprint> localFingerprint = localDescription.fingerprint();

    if (!localFingerprint.has_value()) {
        LOG_ERROR(LogAreaID::Network, "NetherNet answer has no DTLS fingerprint");
        connection->close();
        errorCode = nethernet::ErrorCodeFailedToCreateAnswer;
        return false;
    }

    if (!localDescription.iceUfrag().has_value() || !localDescription.icePwd().has_value()) {
        LOG_ERROR(LogAreaID::Network, "NetherNet answer has no ICE credentials");
        connection->close();
        errorCode = nethernet::ErrorCodeFailedToCreateAnswer;
        return false;
    }

    if (localDescription.role() == rtc::Description::Role::ActPass) {
        LOG_ERROR(LogAreaID::Network, "NetherNet answer resolved to an illegal actpass DTLS role");
        connection->close();
        errorCode = nethernet::ErrorCodeFailedToCreateAnswer;
        return false;
    }

    nethernet::Fingerprint answerFingerprint;
    answerFingerprint.mAlgorithm = rtc::CertificateFingerprint::AlgorithmIdentifier(
            localFingerprint.value().algorithm);
    answerFingerprint.mValue = localFingerprint.value().value;

    const std::vector<nethernet::Fingerprint> localFingerprints(1, answerFingerprint);

    if (!mIdentity.issueToken()) {
        LOG_ERROR(LogAreaID::Network, "Failed to issue the NetherNet server token");
        connection->close();
        errorCode = nethernet::ErrorCodeIdentityVerificationFailed;
        return false;
    }

    const std::string localPayload = nethernet::Description::generateFingerprintsPayload(localFingerprints);
    const std::string assertion = mIdentity.signDetached(localPayload);

    if (assertion.empty()) {
        LOG_ERROR(LogAreaID::Network, "Failed to sign the NetherNet DTLS fingerprints");
        connection->close();
        errorCode = nethernet::ErrorCodeIdentityVerificationFailed;
        return false;
    }

    nethernet::IdentityData localIdentity;
    localIdentity.mAssertionFingerprints = assertion;
    localIdentity.mAssertionToken = mIdentity.getToken();
    localIdentity.mIdpDomain = mIdentity.getDomain();
    localIdentity.mIdpProtocol = "default";

    nethernet::AnswerParameters parameters;
    parameters.mSessionId = randomNetworkID();
    parameters.mUsernameFragment = localDescription.iceUfrag().value();
    parameters.mPassword = localDescription.icePwd().value();
    parameters.mFingerprints = localFingerprints;
    parameters.mSetupRole = setupRoleName(localDescription.role());
    parameters.mEncodedIdentity = nethernet::Description::encodeIdentity(localIdentity);

    for (const rtc::Candidate &candidate: localDescription.candidates())
        parameters.mCandidates.push_back(candidate.candidate());

    if (parameters.mCandidates.empty()) {
        LOG_WARN(LogAreaID::Network, "NetherNet answer carries no ICE candidate, the client cannot connect back");
        connection->close();
        errorCode = nethernet::ErrorCodeFailedToCreateAnswer;
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mPending.push_back(connection);
    }

    answer = nethernet::Description::buildAnswer(parameters);
    return true;
}

void NetherNetInstance::runEvents() {
    if (!mIsHosting)
        return;

    std::vector<std::shared_ptr<nethernet::Connection>> ready;
    std::vector<std::shared_ptr<nethernet::Connection>> dropped;
    std::vector<std::shared_ptr<nethernet::Connection>> finished;

    {
        std::lock_guard<std::mutex> lock(mMutex);

        for (size_t i = 0; i < mPending.size();) {
            const std::shared_ptr<nethernet::Connection> &connection = mPending[i];

            if (connection->isClosed()) {
                mPending.erase(mPending.begin() + (long) i);
                continue;
            }

            if (connection->areChannelsReady()) {
                ready.push_back(connection);
                mPending.erase(mPending.begin() + (long) i);
                continue;
            }

            i++;
        }

        for (auto it = mNegotiating.begin(); it != mNegotiating.end();) {
            if (it->second.expired())
                it = mNegotiating.erase(it);
            else
                ++it;
        }

        for (auto it = mNegotiatingConnections.begin(); it != mNegotiatingConnections.end();) {
            if (it->second.expired())
                it = mNegotiatingConnections.erase(it);
            else
                ++it;
        }

        for (auto it = mPeers.begin(); it != mPeers.end();) {
            if (it->second->isClosed()) {
                dropped.push_back(it->second);
                it = mPeers.erase(it);
                continue;
            }

            ++it;
        }

        // the server already let these go: no closed callback for them
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        for (size_t i = 0; i < mClosing.size();) {
            const ClosingPeer &closing = mClosing[i];
            if (closing.mConnection->isClosed() || closing.mConnection->getBufferedAmount() == 0
                || now >= closing.mDeadline) {
                finished.push_back(closing.mConnection);
                mClosing.erase(mClosing.begin() + (long) i);
                continue;
            }

            i++;
        }
    }

    for (const std::shared_ptr<nethernet::Connection> &connection: finished)
        connection->close();

    for (const std::shared_ptr<nethernet::Connection> &connection: ready) {
        // channels are open, so the ICE pair is chosen: bans and logs go by where the packets come from,
        // not by the network id the peer picked for itself
        connection->resolveRemoteAddress();
        const NetworkIdentifier &id = connection->getNetworkIdentifier();

        if (mConnectionInspector && !mUnofficialClientsAllowed) {
            const int check = mConnectionInspector(connection->getRemoteFingerprint());
            if (check > 0) {
                LOG_WARN(LogAreaID::Network, "NetherNet connection %s closed: client check %d",
                         id.getAddress().c_str(), check);
                connection->close();
                continue;
            }
            if (check < 0)
                LOG_INFO(LogAreaID::Network, "NetherNet connection %s: client note %d", id.getAddress().c_str(),
                         -check);
        }

        if (mConnectionObserver && !mUnofficialClientsAllowed)
            mConnectionObserver(*connection);

        if (mCallbacks != nullptr && !mCallbacks->onValidateIncomingConnection(id)) {
            connection->close();
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(mMutex);
            mPeers[id] = connection;
        }

        if (mCallbacks != nullptr)
            mCallbacks->onNewIncomingConnection(id, connection);
    }

    for (const std::shared_ptr<nethernet::Connection> &connection: dropped) {
        if (mCallbacks != nullptr) {
            mCallbacks->onConnectionClosed(connection->getNetworkIdentifier(), DisconnectFailReason::Disconnected,
                                           toString(DisconnectFailReason::Disconnected));
        }
    }
}
