#include "Network/Client/NetherNetClient.h"

#include "Core/Debug/BedrockLog.h"
#include "Network/ConnectionRequest.h"
#include "Network/Crypto/KeyPair.h"
#include "Network/NetherNet/NetherNetDescription.h"
#include "Network/NetherNet/NetherNetIdentity.h"

#include <openssl/evp.h>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <random>
#include <vector>

namespace {

    const size_t MAX_QUEUED_SIGNALS = 64;
    const unsigned int SIGNAL_ERROR_TIMEOUT_MS = 2000;
    const unsigned int CANDIDATE_SIGNAL_TIMEOUT_MS = 15000;
    const int NEGOTIATION_POLL_MS = 10;
    const char *IDENTITY_PROTOCOL = "default";
    const char *DETACHED_SIGNATURE_HEADER = "{\"alg\":\"ES384\"}";

    unsigned long long randomUint64() {
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

    bool parseErrorCode(const std::string &data, unsigned int &outCode) {
        if (data.empty() || data.size() > 10)
            return false;

        unsigned long long value = 0;

        for (char character: data) {
            if (character < '0' || character > '9')
                return false;

            value = value * 10 + (unsigned long long) (character - '0');
        }

        if (value > 0xffffffffull)
            return false;

        outCode = (unsigned int) value;
        return true;
    }

    std::string signDetached(const KeyPair &key, const std::string &payload) {
        const std::string header = nethernet::Identity::encodeBase64Url(DETACHED_SIGNATURE_HEADER);
        const std::string signingInput = header + "." + nethernet::Identity::encodeBase64Url(payload);
        const std::string signature = key.sign(signingInput);

        if (signature.empty())
            return std::string();

        return header + ".." + nethernet::Identity::encodeBase64Url(signature);
    }

    bool verifyServerIdentity(const nethernet::IdentityData &identity,
                              const std::vector<nethernet::Fingerprint> &fingerprints, std::string &outError) {
        std::string failure;
        EVP_PKEY *publicKey = nethernet::Identity::claimPublicKey(identity.mAssertionToken, failure);

        if (publicKey == nullptr) {
            outError = "verify server identity token: " + failure;
            return false;
        }

        const std::string &token = identity.mAssertionToken;
        const size_t first = token.find('.');
        const size_t second = first == std::string::npos ? std::string::npos : token.find('.', first + 1);

        bool verified = false;

        if (second != std::string::npos) {
            const std::string detachedToken = token.substr(0, first) + ".." + token.substr(second + 1);
            const std::string claims = ConnectionRequest::decodeBase64Url(token.substr(first + 1, second - first - 1));
            verified = !claims.empty() && nethernet::Identity::verifyDetached(publicKey, detachedToken, claims);
        }

        if (!verified) {
            EVP_PKEY_free(publicKey);
            outError = "the server identity token is not self-signed by its cpk claim";
            return false;
        }

        const std::string payload = nethernet::Description::generateFingerprintsPayload(fingerprints);
        verified = nethernet::Identity::verifyDetached(publicKey, identity.mAssertionFingerprints, payload);
        EVP_PKEY_free(publicKey);

        if (!verified) {
            outError = "verify server identity: fingerprints assertion could not be verified";
            return false;
        }

        return true;
    }

}

struct NetherNetClient::State {
    std::mutex mMutex;
    std::condition_variable mSignal;
    std::deque<nethernet::Signal> mSignals;
    std::deque<std::string> mLocalCandidates;
    bool mGatheringComplete = false;
    bool mTransportFailed = false;
    std::string mFailure;
};

NetherNetClient::NetherNetClient()
        : mConnectionID(0), mSubscription(0), mNextCandidateIndex(0), mSubscribed(false), mTrickleIce(true),
          mOfferSent(false), mStarted(false), mConnected(false), mCloseReason(DisconnectFailReason::Unknown) {
}

NetherNetClient::~NetherNetClient() {
    close();
}

bool NetherNetClient::isConnected() const {
    return mConnected.load();
}

DisconnectFailReason NetherNetClient::getCloseReason() const {
    return mCloseReason.load();
}

const std::shared_ptr<nethernet::Connection> &NetherNetClient::getPeer() const {
    return mPeer;
}

const NetworkIdentifier &NetherNetClient::getServerIdentifier() const {
    return mServerId;
}

const std::shared_ptr<nethernet::Signaling> &NetherNetClient::getSignaling() const {
    return mSignaling;
}

bool NetherNetClient::connect(const std::string &networkID, const std::shared_ptr<nethernet::Signaling> &signaling,
                              const NetherNetDialOptions &options, unsigned int timeoutMs,
                              const std::atomic<bool> *cancel, std::string &outError) {
    if (mStarted) {
        outError = "the client is already started";
        return false;
    }

    if (signaling == nullptr) {
        outError = "no NetherNet signaling was provided";
        return false;
    }

    if (signaling->isClosed()) {
        outError = "the NetherNet signaling is closed: " + signaling->getCloseReason();
        return false;
    }

    if (networkID.empty()) {
        outError = "the remote NetherNet network ID is empty";
        return false;
    }

    mStarted = true;
    mSignaling = signaling;
    mOptions = options;
    mRemoteNetworkID = networkID;
    mConnectionID = options.mConnectionID != 0 ? options.mConnectionID : randomUint64();
    mTrickleIce = !options.mDisableTrickleIce && !signaling->isTrickleIceDisabled();
    mNextCandidateIndex = 0;
    mOfferSent = false;

    if (!_negotiate(networkID, timeoutMs, cancel, outError)) {
        LOG_WARN(LogAreaID::Network, "NetherNet dial to %s failed: %s", networkID.c_str(), outError.c_str());
        _teardown();
        mStarted = false;
        return false;
    }

    mCloseReason.store(DisconnectFailReason::Unknown);
    mConnected.store(true);

    LOG_INFO(LogAreaID::Network, "NetherNet connection %llu established with %s", mConnectionID,
             networkID.c_str());
    return true;
}

bool NetherNetClient::_negotiate(const std::string &networkID, unsigned int timeoutMs,
                                 const std::atomic<bool> *cancel, std::string &outError) {
    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::milliseconds(timeoutMs);

    const auto remainingMs = [deadline]() {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                deadline - std::chrono::steady_clock::now()).count();
        return left > 0 ? (unsigned int) left : 0u;
    };

    nethernet::Credentials credentials;
    if (!mSignaling->requestCredentials(credentials, remainingMs(), outError)) {
        outError = "obtain credentials: " + outError;
        return false;
    }

    mState = std::make_shared<State>();

    const std::weak_ptr<State> weakState = mState;
    const unsigned long long connectionID = mConnectionID;

    mSubscription = mSignaling->subscribe([weakState, networkID, connectionID](const nethernet::Signal &signal) {
        if (signal.mConnectionID != connectionID || signal.mNetworkID != networkID)
            return;

        const std::shared_ptr<State> state = weakState.lock();
        if (state == nullptr)
            return;

        {
            std::lock_guard<std::mutex> lock(state->mMutex);

            if (state->mSignals.size() >= MAX_QUEUED_SIGNALS) {
                LOG_WARN(LogAreaID::Network, "Dropping a NetherNet %s signal because the queue is full",
                         signal.mType.c_str());
                return;
            }

            state->mSignals.push_back(signal);
        }

        state->mSignal.notify_all();
    });
    mSubscribed = true;

    rtc::Configuration configuration;
    configuration.mtu = nethernet::NETWORK_MTU;
    configuration.maxMessageSize = nethernet::MAX_MESSAGE_SIZE + 1;
    configuration.disableAutoNegotiation = true;

    for (const nethernet::IceServer &server: credentials.mIceServers) {
        for (const std::string &url: server.mUrls) {
            try {
                rtc::IceServer entry(url);
                entry.username = server.mUsername;
                entry.password = server.mPassword;
                configuration.iceServers.push_back(entry);
            } catch (const std::exception &error) {
                LOG_WARN(LogAreaID::Network, "Ignoring NetherNet ICE server %s: %s", url.c_str(), error.what());
            }
        }
    }

    try {
        mPeerConnection = std::make_shared<rtc::PeerConnection>(configuration);
    } catch (const std::exception &error) {
        outError = std::string("create peer connection: ") + error.what();
        return false;
    }

    mServerId = NetworkIdentifier(networkID, mConnectionID);
    mPeer = std::make_shared<nethernet::Connection>(mServerId, mPeerConnection);

    mPeerConnection->onLocalCandidate([weakState](rtc::Candidate candidate) {
        const std::shared_ptr<State> state = weakState.lock();
        if (state == nullptr)
            return;

        {
            std::lock_guard<std::mutex> lock(state->mMutex);
            state->mLocalCandidates.push_back(candidate.candidate());
        }

        state->mSignal.notify_all();
    });

    mPeerConnection->onGatheringStateChange([weakState](rtc::PeerConnection::GatheringState gatheringState) {
        if (gatheringState != rtc::PeerConnection::GatheringState::Complete)
            return;

        const std::shared_ptr<State> state = weakState.lock();
        if (state == nullptr)
            return;

        {
            std::lock_guard<std::mutex> lock(state->mMutex);
            state->mGatheringComplete = true;
        }

        state->mSignal.notify_all();
    });

    mPeerConnection->onStateChange([weakState](rtc::PeerConnection::State connectionState) {
        if (connectionState != rtc::PeerConnection::State::Failed &&
            connectionState != rtc::PeerConnection::State::Closed)
            return;

        const std::shared_ptr<State> state = weakState.lock();
        if (state == nullptr)
            return;

        {
            std::lock_guard<std::mutex> lock(state->mMutex);

            if (!state->mTransportFailed) {
                state->mTransportFailed = true;
                state->mFailure = connectionState == rtc::PeerConnection::State::Failed
                                  ? "peer connection entered an unrecoverable state: failed"
                                  : "peer connection closed";
            }
        }

        state->mSignal.notify_all();
    });

    mPeerConnection->onDataChannel([weakState](std::shared_ptr<rtc::DataChannel> channel) {
        const std::string label = channel->label();
        channel->close();

        const std::shared_ptr<State> state = weakState.lock();
        if (state == nullptr)
            return;

        {
            std::lock_guard<std::mutex> lock(state->mMutex);

            if (!state->mTransportFailed) {
                state->mTransportFailed = true;
                state->mFailure = "data channel \"" + label + "\" was unexpectedly opened by remote peer";
            }
        }

        state->mSignal.notify_all();
    });

    rtc::DataChannelInit reliableInit;
    rtc::DataChannelInit unreliableInit;
    unreliableInit.reliability.unordered = true;
    unreliableInit.reliability.maxRetransmits = 0;

    std::shared_ptr<rtc::DataChannel> reliable;
    std::shared_ptr<rtc::DataChannel> unreliable;

    try {
        reliable = mPeerConnection->createDataChannel(nethernet::RELIABLE_CHANNEL_LABEL, reliableInit);
        unreliable = mPeerConnection->createDataChannel(nethernet::UNRELIABLE_CHANNEL_LABEL, unreliableInit);
    } catch (const std::exception &error) {
        outError = std::string("create data channels: ") + error.what();
        return false;
    }

    if (!mPeer->attachChannel(reliable) || !mPeer->attachChannel(unreliable)) {
        outError = "data channel created for same reliability parameters";
        return false;
    }

    std::string offer;
    if (!_createOffer(remainingMs(), offer, outError))
        return false;

    nethernet::Signal offerSignal;
    offerSignal.mType = nethernet::SIGNAL_TYPE_OFFER;
    offerSignal.mConnectionID = mConnectionID;
    offerSignal.mData = offer;
    offerSignal.mNetworkID = networkID;

    if (!mSignaling->signal(offerSignal, remainingMs(), outError)) {
        outError = "signal offer: " + outError;
        return false;
    }

    mOfferSent = true;

    bool answered = false;
    std::vector<std::string> earlyCandidates;

    for (;;) {
        if (cancel != nullptr && cancel->load()) {
            outError = "dial cancelled";
            return false;
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            _signalError(answered ? nethernet::ErrorCodeInactivityTimeout
                                  : nethernet::ErrorCodeNegotiationTimeoutWaitingForResponse);
            outError = answered ? "timed out establishing the transports" : "timed out waiting for the answer";
            return false;
        }

        if (mSignaling->isClosed()) {
            outError = "signaling closed: " + mSignaling->getCloseReason();
            return false;
        }

        {
            std::unique_lock<std::mutex> lock(mState->mMutex);

            mState->mSignal.wait_for(lock, std::chrono::milliseconds(NEGOTIATION_POLL_MS), [this]() {
                return !mState->mSignals.empty() || mState->mTransportFailed ||
                       (mTrickleIce && !mState->mLocalCandidates.empty());
            });

            if (mState->mTransportFailed) {
                outError = mState->mFailure;
                return false;
            }
        }

        for (;;) {
            nethernet::Signal signal;

            {
                std::lock_guard<std::mutex> lock(mState->mMutex);

                if (mState->mSignals.empty())
                    break;

                signal = std::move(mState->mSignals.front());
                mState->mSignals.pop_front();
            }

            if (signal.mType == nethernet::SIGNAL_TYPE_ERROR) {
                unsigned int code = 0;

                if (!parseErrorCode(signal.mData, code)) {
                    if (!answered)
                        _signalError(nethernet::ErrorCodeIncomingConnectionIgnored);

                    outError = "handle signal: parse error code: " + signal.mData;
                    return false;
                }

                outError = "remote peer notified connection failure (code: " + std::to_string(code) + ")";
                return false;
            }

            if (signal.mType == nethernet::SIGNAL_TYPE_CANDIDATE) {
                if (answered) {
                    std::string error;
                    if (!_addRemoteCandidate(signal.mData, error))
                        LOG_WARN(LogAreaID::Network, "Error handling a NetherNet candidate: %s", error.c_str());

                    continue;
                }

                try {
                    rtc::Candidate parsed(nethernet::Description::stripCandidatePrefix(signal.mData), "0");
                    (void) parsed;
                } catch (const std::exception &error) {
                    _signalError(nethernet::ErrorCodeIncomingConnectionIgnored);
                    outError = std::string("handle signal: decode candidate: ") + error.what();
                    return false;
                }

                earlyCandidates.push_back(signal.mData);
                continue;
            }

            if (answered)
                continue;

            if (signal.mType != nethernet::SIGNAL_TYPE_ANSWER) {
                _signalError(nethernet::ErrorCodeIncomingConnectionIgnored);
                outError = "handle signal: unknown signal type: " + signal.mType;
                return false;
            }

            int errorCode = 0;
            if (!_acceptAnswer(signal.mData, outError, errorCode)) {
                _signalError(errorCode);
                return false;
            }

            answered = true;

            for (const std::string &candidate: earlyCandidates) {
                if (!_addRemoteCandidate(candidate, outError)) {
                    _signalError(nethernet::ErrorCodeIncomingConnectionIgnored);
                    outError = "handle signal: " + outError;
                    return false;
                }
            }

            earlyCandidates.clear();
        }

        _flushLocalCandidates();

        {
            std::lock_guard<std::mutex> lock(mState->mMutex);

            if (mState->mTransportFailed) {
                outError = mState->mFailure;
                return false;
            }
        }

        if (mPeer->isClosed()) {
            outError = "the connection was closed during negotiation";
            return false;
        }

        if (answered && mPeer->areChannelsReady())
            return true;
    }
}

bool NetherNetClient::_createOffer(unsigned int timeoutMs, std::string &outOffer, std::string &outError) {
    try {
        mPeerConnection->setLocalDescription(rtc::Description::Type::Offer);
    } catch (const std::exception &error) {
        outError = std::string("create offer: ") + error.what();
        return false;
    }

    if (!mTrickleIce) {
        std::unique_lock<std::mutex> lock(mState->mMutex);

        if (mPeerConnection->gatheringState() == rtc::PeerConnection::GatheringState::Complete)
            mState->mGatheringComplete = true;

        mState->mSignal.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]() {
            return mState->mGatheringComplete || mState->mTransportFailed;
        });

        if (!mState->mGatheringComplete) {
            outError = "gather local candidates (non-trickle ICE): timed out";
            return false;
        }

        mState->mLocalCandidates.clear();
    }

    const std::optional<rtc::Description> local = mPeerConnection->localDescription();
    if (!local.has_value()) {
        outError = "encode offer: the peer connection produced no local description";
        return false;
    }

    const rtc::Description &description = local.value();
    const std::optional<rtc::CertificateFingerprint> fingerprint = description.fingerprint();

    if (!fingerprint.has_value()) {
        outError = "encode offer: local DTLS parameters has no fingerprints";
        return false;
    }

    if (!description.iceUfrag().has_value() || !description.icePwd().has_value()) {
        outError = "encode offer: local description has no ICE credentials";
        return false;
    }

    nethernet::Fingerprint localFingerprint;
    localFingerprint.mAlgorithm = rtc::CertificateFingerprint::AlgorithmIdentifier(fingerprint.value().algorithm);
    localFingerprint.mValue = fingerprint.value().value;

    nethernet::AnswerParameters parameters;
    parameters.mSessionId = randomUint64();
    parameters.mUsernameFragment = description.iceUfrag().value();
    parameters.mPassword = description.icePwd().value();
    parameters.mFingerprints.push_back(localFingerprint);
    parameters.mSetupRole = setupRoleName(description.role());

    if (!mTrickleIce) {
        for (const rtc::Candidate &candidate: description.candidates())
            parameters.mCandidates.push_back(candidate.candidate());
    }

    if (mOptions.mIdentityKey != nullptr && !mOptions.mIdentityToken.empty()) {
        const std::string payload = nethernet::Description::generateFingerprintsPayload(parameters.mFingerprints);
        const std::string assertion = signDetached(*mOptions.mIdentityKey, payload);

        if (assertion.empty()) {
            outError = "generate identity assertion: could not sign DTLS fingerprints";
            return false;
        }

        nethernet::IdentityData identity;
        identity.mAssertionFingerprints = assertion;
        identity.mAssertionToken = mOptions.mIdentityToken;
        identity.mIdpDomain = mOptions.mIdentityDomain;
        identity.mIdpProtocol = IDENTITY_PROTOCOL;
        parameters.mEncodedIdentity = nethernet::Description::encodeIdentity(identity);
    }

    mUsernameFragment = parameters.mUsernameFragment;
    outOffer = nethernet::Description::buildAnswer(parameters);
    return true;
}

bool NetherNetClient::_acceptAnswer(const std::string &answer, std::string &outError, int &outErrorCode) {
    const std::vector<nethernet::Fingerprint> fingerprints = nethernet::Description::parseFingerprints(answer);

    if (fingerprints.empty()) {
        outErrorCode = nethernet::ErrorCodeFailedToSetRemoteDescription;
        outError = "parse answer: missing fingerprint attribute";
        return false;
    }

    nethernet::IdentityData identity;
    std::string failure;

    if (nethernet::Description::parseIdentity(answer, identity, failure)) {
        if (!identity.isValid()) {
            outErrorCode = nethernet::ErrorCodeFailedToSetRemoteDescription;
            outError = "parse answer: malformed identity attribute";
            return false;
        }

        if (!verifyServerIdentity(identity, fingerprints, outError)) {
            outErrorCode = nethernet::ErrorCodeIdentityVerificationFailed;
            return false;
        }
    } else if (!failure.empty()) {
        outErrorCode = nethernet::ErrorCodeFailedToSetRemoteDescription;
        outError = "parse answer: " + failure;
        return false;
    } else if (!mOptions.mAllowIdentitylessServer) {
        outErrorCode = nethernet::ErrorCodeIdentityVerificationFailed;
        outError = "identityless answer SDP not allowed";
        return false;
    }

    try {
        mPeerConnection->setRemoteDescription(rtc::Description(answer, rtc::Description::Type::Answer));
    } catch (const std::exception &error) {
        outErrorCode = nethernet::ErrorCodeFailedToSetRemoteDescription;
        outError = std::string("apply answer: ") + error.what();
        return false;
    }

    return true;
}

bool NetherNetClient::_addRemoteCandidate(const std::string &data, std::string &outError) {
    try {
        mPeerConnection->addRemoteCandidate(rtc::Candidate(nethernet::Description::stripCandidatePrefix(data), "0"));
    } catch (const std::exception &error) {
        outError = std::string("add remote candidate: ") + error.what();
        return false;
    }

    return true;
}

void NetherNetClient::_flushLocalCandidates() {
    if (!mTrickleIce || !mOfferSent || mState == nullptr || mSignaling == nullptr)
        return;

    for (;;) {
        std::string candidate;

        {
            std::lock_guard<std::mutex> lock(mState->mMutex);

            if (mState->mLocalCandidates.empty())
                return;

            candidate = std::move(mState->mLocalCandidates.front());
            mState->mLocalCandidates.pop_front();
        }

        nethernet::Signal signal;
        signal.mType = nethernet::SIGNAL_TYPE_CANDIDATE;
        signal.mConnectionID = mConnectionID;
        signal.mData = nethernet::Description::formatCandidate((int) mNextCandidateIndex++, candidate,
                                                                mUsernameFragment);
        signal.mNetworkID = mRemoteNetworkID;

        std::string error;
        if (mSignaling->signal(signal, CANDIDATE_SIGNAL_TIMEOUT_MS, error))
            continue;

        _signalError(nethernet::ErrorCodeSignalingFailedToSend);

        {
            std::lock_guard<std::mutex> lock(mState->mMutex);

            if (!mState->mTransportFailed) {
                mState->mTransportFailed = true;
                mState->mFailure = "signal candidate: " + error;
            }
        }

        return;
    }
}

void NetherNetClient::_signalError(int code) {
    if (code == 0 || mSignaling == nullptr)
        return;

    nethernet::Signal signal;
    signal.mType = nethernet::SIGNAL_TYPE_ERROR;
    signal.mConnectionID = mConnectionID;
    signal.mData = std::to_string(code);
    signal.mNetworkID = mRemoteNetworkID;

    std::string error;
    if (!mSignaling->signal(signal, SIGNAL_ERROR_TIMEOUT_MS, error))
        LOG_WARN(LogAreaID::Network, "Could not signal NetherNet error %d: %s", code, error.c_str());
}

void NetherNetClient::runEvents() {
    if (!mStarted || !mConnected.load() || mState == nullptr)
        return;

    for (;;) {
        nethernet::Signal signal;

        {
            std::lock_guard<std::mutex> lock(mState->mMutex);

            if (mState->mSignals.empty())
                break;

            signal = std::move(mState->mSignals.front());
            mState->mSignals.pop_front();
        }

        if (signal.mType == nethernet::SIGNAL_TYPE_CANDIDATE) {
            std::string error;
            if (!_addRemoteCandidate(signal.mData, error))
                LOG_WARN(LogAreaID::Network, "Error handling a NetherNet candidate: %s", error.c_str());

            continue;
        }

        if (signal.mType == nethernet::SIGNAL_TYPE_ERROR) {
            LOG_WARN(LogAreaID::Network, "NetherNet remote peer notified connection failure (code: %s)",
                     signal.mData.c_str());
            _markClosed(DisconnectFailReason::Disconnected);
            return;
        }
    }

    _flushLocalCandidates();

    bool failed;
    std::string failure;

    {
        std::lock_guard<std::mutex> lock(mState->mMutex);
        failed = mState->mTransportFailed;

        if (failed)
            failure = mState->mFailure;
    }

    if (failed) {
        LOG_WARN(LogAreaID::Network, "NetherNet connection %s closed: %s", mRemoteNetworkID.c_str(),
                 failure.c_str());
        _markClosed(DisconnectFailReason::Disconnected);
        return;
    }

    if (mPeer != nullptr && mPeer->isClosed())
        _markClosed(DisconnectFailReason::Disconnected);
}

void NetherNetClient::_markClosed(DisconnectFailReason reason) {
    if (mConnected.exchange(false))
        mCloseReason.store(reason);

    _teardown();
}

void NetherNetClient::_teardown() {
    if (mSubscribed && mSignaling != nullptr) {
        mSignaling->unsubscribe(mSubscription);
        mSubscribed = false;
    }

    if (mPeer != nullptr)
        mPeer->close();

    if (mPeerConnection != nullptr) {
        mPeerConnection->close();
        mPeerConnection.reset();
    }

    if (mOptions.mCloseSignalingOnClose && mSignaling != nullptr)
        mSignaling->close();
}

void NetherNetClient::close() {
    if (!mStarted)
        return;

    _markClosed(DisconnectFailReason::Disconnected);
    mStarted = false;
}
