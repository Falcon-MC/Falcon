#pragma once

#include <string>
#include <vector>

/**
 * Verifies the identity a client sends at login, either the signed authentication token or
 * the legacy certificate chain, and extracts the player's name, XUID and public key. A
 * login that is readable but not authenticated is accepted with isSigned() false.
 */
class LoginChainVerifier {
public:
    bool verify(const std::string &authJwt, const std::string &clientJwt);

    bool isSigned() const { return mSigned; }

    const std::string &getIdentity() const { return mIdentity; }

    const std::string &getXuid() const { return mXuid; }

    const std::string &getDisplayName() const { return mDisplayName; }

    const std::string &getTitleId() const { return mTitleId; }

    const std::string &getIdentityPublicKey() const { return mIdentityPublicKey; }

    const std::string &getFailureReason() const { return mFailureReason; }

private:
    static std::vector<std::string> _extractChain(const std::string &authJwt);

    static std::string _extractToken(const std::string &authJwt);

    static std::string _xuidToUuid(const std::string &xuid);

    static bool _splitJwt(const std::string &jwt, std::string &header, std::string &payload, std::string &signature);

    static std::string _extractObject(const std::string &json, const std::string &key);

    static long long _findJsonLong(const std::string &json, const std::string &key, long long fallback);

    bool _fail(const std::string &reason);

    bool _checkExpiry(const std::string &payloadJson);

    bool _verifyClientData(const std::string &clientJwt, const std::string &expectedKeyBase64);

    bool _verifyOpenIdToken(const std::string &token, const std::string &clientJwt);

    bool _verifySelfSignedToken(const std::string &token, const std::string &clientJwt);

    bool _verifyChain(const std::vector<std::string> &chain);

    void _extractIdentity(const std::string &payload);

    bool mSigned = false;
    std::string mIdentity;
    std::string mXuid;
    std::string mDisplayName;
    std::string mTitleId;
    std::string mIdentityPublicKey;
    std::string mFailureReason;
};
