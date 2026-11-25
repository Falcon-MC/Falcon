#pragma once

#include "Network/Acme/AcmeChallengeStore.h"
#include "Network/Acme/AcmeCrypto.h"
#include "Network/Http/HttpClient.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace acme {

    enum class ChallengeType {
        TlsAlpn01,
        Http01
    };

    const char *toString(ChallengeType type);

    bool parseChallengeType(const std::string &text, ChallengeType &outType);

    struct ClientSettings {
        std::string mDirectoryUrl;
        std::string mDomain;
        std::string mEmail;
        ChallengeType mChallenge = ChallengeType::TlsAlpn01;
        // test CA of a local ACME server, empty in production
        std::string mExtraRootsFile;
        int mRequestTimeoutMs = 15000;
        // validation and finalization polls before giving up on an order
        int mMaxPolls = 30;
    };

    struct Outcome {
        bool mIssued = false;
        std::string mChainPem;
        std::string mError;
        bool mRateLimited = false;
        // from Retry-After on a refusal, 0 when the server gave none
        int64_t mRetryAfterSeconds = 0;
    };

    // RFC 8555 order flow, blocking: run it off the main thread
    class Client {
    public:
        Client(const ClientSettings &settings, std::shared_ptr<EcKey> accountKey, ChallengeStore &challenges,
               const std::atomic<bool> *cancel);

        Outcome issue(const EcKey &certificateKey);

    private:
        struct Reply {
            HttpResponse mResponse;
            std::string mError;
            bool mSent = false;
        };

        bool _loadDirectory(Outcome &outcome);

        bool _fetchNonce(Outcome &outcome);

        bool _post(const std::string &url, const std::string &payload, Reply &reply, Outcome &outcome,
                   const char *accept = nullptr);

        bool _register(Outcome &outcome);

        bool _authorize(const std::string &authorizationUrl, Outcome &outcome);

        bool _pollStatus(const std::string &url, const char *pending, const char *done, std::string &outBody,
                         Outcome &outcome);

        bool _wait(int64_t seconds);

        void _readProblem(const HttpResponse &response, const char *step, Outcome &outcome);

        ClientSettings mSettings;
        std::shared_ptr<EcKey> mAccountKey;
        ChallengeStore &mChallenges;
        const std::atomic<bool> *mCancel;

        std::string mNewNonceUrl;
        std::string mNewAccountUrl;
        std::string mNewOrderUrl;
        std::string mNonce;
        std::string mAccountUrl;
    };

}
