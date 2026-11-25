#pragma once

#include <string>
#include <vector>

namespace nethernet {
    struct Fingerprint {
        std::string mAlgorithm;
        std::string mValue;
    };

    struct IdentityData {
        std::string mAssertionFingerprints;
        std::string mAssertionToken;
        std::string mIdpDomain;
        std::string mIdpProtocol;

        bool isValid() const;
    };

    struct AnswerParameters {
        unsigned long long mSessionId = 0;
        std::string mUsernameFragment;
        std::string mPassword;
        std::vector<Fingerprint> mFingerprints;
        std::string mSetupRole;
        std::vector<std::string> mCandidates;
        std::string mEncodedIdentity;
    };

    class Description {
    public:
        static std::vector<Fingerprint> parseFingerprints(const std::string &sdp);

        static bool parseIdentity(const std::string &sdp, IdentityData &out, std::string &failure);

        // zero when the offer is one the game could have written, else a number for the operator's
        // log: what gave the client away is never named
        static int inspectOffer(const std::string &sdp);

        static std::string generateFingerprintsPayload(const std::vector<Fingerprint> &fingerprints);

        static std::string encodeIdentity(const IdentityData &identity);

        static std::string formatCandidate(int networkId, const std::string &candidate,
                                           const std::string &usernameFragment);

        static std::string buildAnswer(const AnswerParameters &parameters);

        static std::string stripCandidatePrefix(const std::string &value);
    };
}
