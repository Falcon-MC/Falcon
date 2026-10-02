#include "Network/LoginChainVerifier.h"

#include "Network/AuthKeyProvider.h"
#include "Network/ConnectionRequest.h"
#include "Core/Debug/BedrockLog.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/bn.h>
#include <openssl/x509.h>

namespace {

    const char *MOJANG_PUBLIC_KEY_BASE64 =
            "MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAECRXueJeTDqNRRgJi/vlRufByu/2G0i2Ebt6YMar5QX/R0DIIyrJMcUpruK4QveTfJSTp3Shlq4Gk34cD/4GUWwkv0DVuzeuB+tXija7HBxii03NHDbPAD0AKnLr2wdAp";

    const char *MOJANG_AUDIENCE = "api://auth-minecraft-services/multiplayer";

    const long long CLOCK_DRIFT_MAX = 60;

    const int AUTHENTICATION_TYPE_FULL = 0;
    const int AUTHENTICATION_TYPE_SELF_SIGNED = 2;

    EVP_PKEY *parseKey(const std::string &base64) {
        const std::string der = ConnectionRequest::decodeBase64Url(base64);
        if (der.empty())
            return nullptr;

        const unsigned char *pointer = reinterpret_cast<const unsigned char *>(der.data());
        return d2i_PUBKEY(nullptr, &pointer, (long) der.size());
    }

    bool keyEquals(EVP_PKEY *left, EVP_PKEY *right) {
        if (left == nullptr || right == nullptr)
            return false;

        unsigned char *leftDer = nullptr;
        unsigned char *rightDer = nullptr;

        const int leftLength = i2d_PUBKEY(left, &leftDer);
        const int rightLength = i2d_PUBKEY(right, &rightDer);

        bool equal = false;
        if (leftLength > 0 && leftLength == rightLength)
            equal = std::memcmp(leftDer, rightDer, (size_t) leftLength) == 0;

        OPENSSL_free(leftDer);
        OPENSSL_free(rightDer);
        return equal;
    }

    /**
     * JWS encodes an ES384 signature as raw r || s (48 bytes each), while OpenSSL only
     * verifies the DER-encoded ECDSA-Sig-Value, so it has to be re-encoded first.
     */
    std::string joseToDer(const std::string &signature) {
        if (signature.size() != 96)
            return std::string();

        const unsigned char *raw = reinterpret_cast<const unsigned char *>(signature.data());

        BIGNUM *r = BN_bin2bn(raw, 48, nullptr);
        BIGNUM *s = BN_bin2bn(raw + 48, 48, nullptr);
        if (r == nullptr || s == nullptr) {
            BN_free(r);
            BN_free(s);
            return std::string();
        }

        ECDSA_SIG *sig = ECDSA_SIG_new();
        if (sig == nullptr) {
            BN_free(r);
            BN_free(s);
            return std::string();
        }

        ECDSA_SIG_set0(sig, r, s);

        unsigned char *der = nullptr;
        const int length = i2d_ECDSA_SIG(sig, &der);

        std::string out;
        if (length > 0)
            out.assign(reinterpret_cast<const char *>(der), (size_t) length);

        OPENSSL_free(der);
        ECDSA_SIG_free(sig);
        return out;
    }

    bool verifyDigest(EVP_PKEY *key, const EVP_MD *digest, const std::string &message, const std::string &signature) {
        if (key == nullptr || digest == nullptr || signature.empty())
            return false;

        EVP_MD_CTX *context = EVP_MD_CTX_new();
        if (context == nullptr)
            return false;

        bool valid = false;
        if (EVP_DigestVerifyInit(context, nullptr, digest, nullptr, key) == 1 &&
            EVP_DigestVerifyUpdate(context, message.data(), message.size()) == 1) {
            const int result = EVP_DigestVerifyFinal(
                    context, reinterpret_cast<const unsigned char *>(signature.data()), signature.size());
            valid = result == 1;
        }

        EVP_MD_CTX_free(context);
        return valid;
    }

    bool verifyEs384(EVP_PKEY *key, const std::string &message, const std::string &derSignature) {
        return verifyDigest(key, EVP_sha384(), message, derSignature);
    }

    bool verifyRs256(EVP_PKEY *key, const std::string &message, const std::string &rawSignature) {
        return verifyDigest(key, EVP_sha256(), message, rawSignature);
    }

}

std::vector<std::string> LoginChainVerifier::_extractChain(const std::string &authJwt) {
    std::vector<std::string> chain;

    const std::string pattern = "\"chain\"";
    size_t position = authJwt.find(pattern);
    if (position == std::string::npos)
        return chain;

    position = authJwt.find('[', position + pattern.size());
    if (position == std::string::npos)
        return chain;

    const size_t end = authJwt.find(']', position);
    if (end == std::string::npos)
        return chain;

    std::string token;
    int dots = 0;

    for (size_t i = position; i <= end; i++) {
        const char c = authJwt[i];
        const bool isTokenChar = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                 (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';

        if (isTokenChar) {
            token.push_back(c);
            if (c == '.')
                dots++;
            continue;
        }

        if (!token.empty()) {
            if (dots == 2)
                chain.push_back(token);
            token.clear();
            dots = 0;
        }
    }

    if (!token.empty() && dots == 2)
        chain.push_back(token);

    return chain;
}

bool LoginChainVerifier::_splitJwt(const std::string &jwt, std::string &header, std::string &payload,
                                   std::string &signature) {
    const size_t first = jwt.find('.');
    if (first == std::string::npos)
        return false;

    const size_t second = jwt.find('.', first + 1);
    if (second == std::string::npos)
        return false;

    header = jwt.substr(0, first);
    payload = jwt.substr(first + 1, second - first - 1);
    signature = jwt.substr(second + 1);

    return !header.empty() && !payload.empty() && !signature.empty() &&
           jwt.find('.', second + 1) == std::string::npos;
}

std::string LoginChainVerifier::_extractObject(const std::string &json, const std::string &key) {
    const std::string pattern = "\"" + key + "\"";
    size_t position = json.find(pattern);
    if (position == std::string::npos)
        return std::string();

    position = json.find('{', position + pattern.size());
    if (position == std::string::npos)
        return std::string();

    int depth = 0;
    for (size_t i = position; i < json.size(); i++) {
        if (json[i] == '{')
            depth++;
        else if (json[i] == '}') {
            depth--;
            if (depth == 0)
                return json.substr(position, i - position + 1);
        }
    }

    return std::string();
}

long long LoginChainVerifier::_findJsonLong(const std::string &json, const std::string &key, long long fallback) {
    const std::string pattern = "\"" + key + "\"";

    size_t position = json.find(pattern);
    if (position == std::string::npos)
        return fallback;

    position = json.find(':', position + pattern.size());
    if (position == std::string::npos)
        return fallback;

    position++;
    while (position < json.size() && (json[position] == ' ' || json[position] == '\t'))
        position++;

    bool negative = false;
    if (position < json.size() && json[position] == '-') {
        negative = true;
        position++;
    }

    bool hasDigit = false;
    long long value = 0;
    while (position < json.size() && json[position] >= '0' && json[position] <= '9') {
        value = value * 10 + (json[position] - '0');
        position++;
        hasDigit = true;
    }

    if (!hasDigit)
        return fallback;

    return negative ? -value : value;
}

bool LoginChainVerifier::_fail(const std::string &reason) {
    mFailureReason = reason;
    mSigned = false;
    return false;
}

bool LoginChainVerifier::_checkExpiry(const std::string &payloadJson) {
    const long long now = (long long) std::time(nullptr);

    const long long notBefore = _findJsonLong(payloadJson, "nbf", 0);
    if (notBefore != 0 && notBefore > now + CLOCK_DRIFT_MAX)
        return _fail("Your login token is not valid yet, check your system clock");

    const long long expiry = _findJsonLong(payloadJson, "exp", 0);
    if (expiry != 0 && expiry < now - CLOCK_DRIFT_MAX)
        return _fail("Your login token has expired, please reconnect");

    return true;
}

void LoginChainVerifier::_extractIdentity(const std::string &payload) {
    const std::string extraData = _extractObject(payload, "extraData");
    const std::string &source = extraData.empty() ? payload : extraData;

    mDisplayName = ConnectionRequest::findJsonString(source, "displayName");
    mIdentity = ConnectionRequest::findJsonString(source, "identity");
    mXuid = ConnectionRequest::findJsonString(source, "XUID");
    mTitleId = ConnectionRequest::findJsonString(source, "titleId");
}

/**
 * Legacy three-link certificate chain. Each link must be signed by the identityPublicKey of
 * the previous one (the first by its own x5u key), and the second link must be signed by the
 * root authentication key; the identity is read from the last link.
 */
bool LoginChainVerifier::_verifyChain(const std::vector<std::string> &chain) {
    EVP_PKEY *currentKey = nullptr;
    EVP_PKEY *mojangKey = parseKey(MOJANG_PUBLIC_KEY_BASE64);
    if (mojangKey == nullptr) {
        LOG_ERROR(LogAreaID::Network, "Failed to parse the Mojang root public key");
        return _fail("The server could not verify your login");
    }

    bool ok = true;
    std::string lastPayload;

    for (size_t i = 0; i < chain.size(); i++) {
        std::string header;
        std::string payloadPart;
        std::string signaturePart;

        if (!_splitJwt(chain[i], header, payloadPart, signaturePart)) {
            LOG_WARN(LogAreaID::Network, "Login chain entry %u is not a valid JWT", (unsigned) i);
            ok = false;
            break;
        }

        const std::string headerJson = ConnectionRequest::decodeBase64Url(header);
        const std::string x5u = ConnectionRequest::findJsonString(headerJson, "x5u");

        EVP_PKEY *expectedKey = parseKey(x5u);
        if (expectedKey == nullptr) {
            LOG_WARN(LogAreaID::Network, "Login chain entry %u has an invalid x5u key", (unsigned) i);
            ok = false;
            break;
        }

        if (currentKey == nullptr) {
            currentKey = expectedKey;
        } else if (!keyEquals(currentKey, expectedKey)) {
            LOG_WARN(LogAreaID::Network, "Received broken login chain at entry %u", (unsigned) i);
            EVP_PKEY_free(expectedKey);
            ok = false;
            break;
        } else {
            EVP_PKEY_free(expectedKey);
        }

        const std::string message = header + "." + payloadPart;
        const std::string derSignature = joseToDer(ConnectionRequest::decodeBase64Url(signaturePart));

        if (!verifyEs384(currentKey, message, derSignature)) {
            LOG_WARN(LogAreaID::Network, "Login chain signature does not match content at entry %u", (unsigned) i);
            ok = false;
            break;
        }

        if (i == 1 && !keyEquals(currentKey, mojangKey)) {
            LOG_WARN(LogAreaID::Network, "The login chain is not signed by Mojang");
            ok = false;
            break;
        }

        const std::string payloadJson = ConnectionRequest::decodeBase64Url(payloadPart);
        lastPayload = payloadJson;

        const std::string identityPublicKey = ConnectionRequest::findJsonString(payloadJson, "identityPublicKey");
        EVP_PKEY *nextKey = parseKey(identityPublicKey);

        EVP_PKEY_free(currentKey);
        currentKey = nextKey;

        if (currentKey == nullptr && i + 1 < chain.size()) {
            LOG_WARN(LogAreaID::Network, "Login chain entry %u has an invalid identityPublicKey", (unsigned) i);
            ok = false;
            break;
        }
    }

    EVP_PKEY_free(currentKey);
    EVP_PKEY_free(mojangKey);

    if (!ok || lastPayload.empty())
        return _fail("Your login chain could not be verified");

    if (!_checkExpiry(lastPayload))
        return false;

    _extractIdentity(lastPayload);
    mIdentityPublicKey = ConnectionRequest::findJsonString(lastPayload, "identityPublicKey");
    mSigned = true;
    return true;
}

std::string LoginChainVerifier::_extractToken(const std::string &authJwt) {
    return ConnectionRequest::findJsonString(authJwt, "Token");
}

bool LoginChainVerifier::_verifyClientData(const std::string &clientJwt, const std::string &expectedKeyBase64) {
    std::string header;
    std::string payloadPart;
    std::string signaturePart;

    if (!_splitJwt(clientJwt, header, payloadPart, signaturePart))
        return _fail("Your client data token is malformed");

    const std::string headerJson = ConnectionRequest::decodeBase64Url(header);
    const std::string x5u = ConnectionRequest::findJsonString(headerJson, "x5u");

    if (x5u.empty())
        return _fail("Your client data token has no signing key");

    EVP_PKEY *headerKey = parseKey(x5u);
    if (headerKey == nullptr)
        return _fail("Your client data token has an invalid signing key");

    EVP_PKEY *expectedKey = parseKey(expectedKeyBase64);
    if (expectedKey == nullptr) {
        EVP_PKEY_free(headerKey);
        return _fail("Your login token carries an invalid client public key");
    }

    if (!keyEquals(headerKey, expectedKey)) {
        EVP_PKEY_free(headerKey);
        EVP_PKEY_free(expectedKey);
        return _fail("Your client data token was not signed by the announced key");
    }

    EVP_PKEY_free(expectedKey);

    const std::string message = header + "." + payloadPart;
    const std::string derSignature = joseToDer(ConnectionRequest::decodeBase64Url(signaturePart));
    const bool valid = verifyEs384(headerKey, message, derSignature);

    EVP_PKEY_free(headerKey);

    if (!valid)
        return _fail("Your client data token has an invalid signature");

    return true;
}

bool LoginChainVerifier::_verifyOpenIdToken(const std::string &token, const std::string &clientJwt) {
    std::string header;
    std::string payloadPart;
    std::string signaturePart;

    if (!_splitJwt(token, header, payloadPart, signaturePart))
        return _fail("Your login token is malformed");

    const std::string headerJson = ConnectionRequest::decodeBase64Url(header);
    const std::string payloadJson = ConnectionRequest::decodeBase64Url(payloadPart);

    const std::string algorithm = ConnectionRequest::findJsonString(headerJson, "alg");
    const std::string keyId = ConnectionRequest::findJsonString(headerJson, "kid");

    mXuid = ConnectionRequest::findJsonString(payloadJson, "xid");
    mDisplayName = ConnectionRequest::findJsonString(payloadJson, "xname");
    mTitleId = ConnectionRequest::findJsonString(payloadJson, "tid");
    mIdentityPublicKey = ConnectionRequest::findJsonString(payloadJson, "cpk");
    mIdentity = _xuidToUuid(mXuid);

    if (mDisplayName.empty() || mXuid.empty())
        return _fail("Your login token does not carry an Xbox Live identity");

    if (algorithm != "RS256")
        return _fail("Your login token uses an unsupported signature algorithm");

    if (keyId.empty())
        return _fail("Your login token does not announce a signing key");

    std::string issuer;
    EVP_PKEY *key = AuthKeyProvider::getInstance().acquireKey(keyId, issuer);

    // An unknown key id is not fatal here: the login is left unsigned, and the login
    // handler rejects unsigned players when online mode is on.
    if (key == nullptr) {
        LOG_WARN(LogAreaID::Network, "No authentication key matches the key id %s, the login stays unverified",
                 keyId.c_str());
        mSigned = false;
        return true;
    }

    const std::string message = header + "." + payloadPart;
    const bool valid = verifyRs256(key, message, ConnectionRequest::decodeBase64Url(signaturePart));

    EVP_PKEY_free(key);

    if (!valid)
        return _fail("Your login token has an invalid signature");

    if (!issuer.empty() && ConnectionRequest::findJsonString(payloadJson, "iss") != issuer)
        return _fail("Your login token was issued by an unexpected authority");

    if (ConnectionRequest::findJsonString(payloadJson, "aud") != MOJANG_AUDIENCE)
        return _fail("Your login token was not issued for multiplayer");

    if (!_checkExpiry(payloadJson))
        return false;

    if (mIdentityPublicKey.empty())
        return _fail("Your login token does not carry a client public key");

    if (!_verifyClientData(clientJwt, mIdentityPublicKey))
        return false;

    mSigned = true;
    return true;
}

bool LoginChainVerifier::_verifySelfSignedToken(const std::string &token, const std::string &clientJwt) {
    std::string header;
    std::string payloadPart;
    std::string signaturePart;

    if (!_splitJwt(token, header, payloadPart, signaturePart))
        return _fail("Your login token is malformed");

    const std::string payloadJson = ConnectionRequest::decodeBase64Url(payloadPart);

    mDisplayName = ConnectionRequest::findJsonString(payloadJson, "xname");
    mIdentity = ConnectionRequest::findJsonString(payloadJson, "leguuid");
    mIdentityPublicKey = ConnectionRequest::findJsonString(payloadJson, "cpk");
    mXuid.clear();
    mTitleId.clear();

    if (mDisplayName.empty())
        return _fail("Your login token does not carry a name");

    if (mIdentityPublicKey.empty())
        return _fail("Your login token does not carry a client public key");

    EVP_PKEY *selfSignedKey = parseKey(mIdentityPublicKey);
    if (selfSignedKey == nullptr)
        return _fail("Your login token carries an invalid client public key");

    const std::string message = header + "." + payloadPart;
    const std::string derSignature = joseToDer(ConnectionRequest::decodeBase64Url(signaturePart));
    const bool valid = verifyEs384(selfSignedKey, message, derSignature);

    EVP_PKEY_free(selfSignedKey);

    if (!valid)
        return _fail("Your login token has an invalid signature");

    if (ConnectionRequest::findJsonString(payloadJson, "aud") != MOJANG_AUDIENCE)
        return _fail("Your login token was not issued for multiplayer");

    if (!_checkExpiry(payloadJson))
        return false;

    if (!_verifyClientData(clientJwt, mIdentityPublicKey))
        return false;

    mSigned = false;
    return true;
}

/**
 * Derives a stable name-based (version 3, MD5) UUID from the XUID, because the token-based
 * login identifies the player by XUID and carries no identity UUID of its own.
 */
std::string LoginChainVerifier::_xuidToUuid(const std::string &xuid) {
    if (xuid.empty())
        return std::string();

    const std::string source = "pocket-auth-1-xuid:" + xuid;

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int length = 0;

    if (EVP_Digest(source.data(), source.size(), digest, &length, EVP_md5(), nullptr) != 1 || length != 16)
        return std::string();

    digest[6] = (unsigned char) ((digest[6] & 0x0f) | 0x30);
    digest[8] = (unsigned char) ((digest[8] & 0x3f) | 0x80);

    char buffer[40];
    snprintf(buffer, sizeof(buffer), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7],
             digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]);

    return std::string(buffer);
}

bool LoginChainVerifier::verify(const std::string &authJwt, const std::string &clientJwt) {
    mSigned = false;
    mIdentity.clear();
    mXuid.clear();
    mDisplayName.clear();
    mTitleId.clear();
    mIdentityPublicKey.clear();
    mFailureReason.clear();

    const std::string token = _extractToken(authJwt);

    if (!token.empty()) {
        const int authenticationType = ConnectionRequest::findJsonInt(authJwt, "AuthenticationType",
                                                                     AUTHENTICATION_TYPE_FULL);

        if (authenticationType == AUTHENTICATION_TYPE_SELF_SIGNED)
            return _verifySelfSignedToken(token, clientJwt);

        return _verifyOpenIdToken(token, clientJwt);
    }

    const std::vector<std::string> chain = _extractChain(authJwt);

    // A single self-signed link is what an offline client sends; it is accepted but the
    // player is marked unsigned so online-mode checks can reject it later.
    if (chain.size() == 1) {
        std::string header;
        std::string payloadPart;
        std::string signaturePart;

        if (!_splitJwt(chain[0], header, payloadPart, signaturePart))
            return _fail("Your login chain is malformed");

        const std::string payloadJson = ConnectionRequest::decodeBase64Url(payloadPart);
        _extractIdentity(payloadJson);
        mIdentityPublicKey = ConnectionRequest::findJsonString(payloadJson, "identityPublicKey");
        mSigned = false;

        if (mDisplayName.empty() && mIdentity.empty())
            return _fail("Your login chain does not carry an identity");

        return true;
    }

    if (chain.size() == 3)
        return _verifyChain(chain);

    LOG_WARN(LogAreaID::Network, "Unexpected login chain length %u", (unsigned) chain.size());
    return _fail("Your login chain has an unexpected length");
}
