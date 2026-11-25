#include "Network/NetherNet/NetherNetIdentity.h"

#include "Core/Json/Json.h"
#include "Network/ConnectionRequest.h"

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/x509.h>

#include <ctime>
#include <memory>
#include <utility>
#include <vector>

namespace nethernet {

    namespace {

        const char *BASE64_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        const char *BASE64_URL_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        const size_t ES384_COORDINATE_SIZE = 48;
        const long long TOKEN_LIFETIME_SECONDS = 60;
        const unsigned char P384_PUBLIC_KEY_PREFIX[] = {
                0x30, 0x76, 0x30, 0x10, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01,
                0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x22, 0x03, 0x62, 0x00, 0x04
        };

        std::string publicKeyFromJsonWebKey(const json::Value &key) {
            const json::Value *type = key.get("kty");
            const json::Value *curve = key.get("crv");
            const json::Value *x = key.get("x");
            const json::Value *y = key.get("y");
            if (type == nullptr || curve == nullptr || x == nullptr || y == nullptr || !type->isString() ||
                !curve->isString() || !x->isString() || !y->isString())
                return std::string();

            if (type->mString != "EC" || curve->mString != "P-384")
                return std::string();

            const std::string xBytes = ConnectionRequest::decodeBase64Url(x->mString);
            const std::string yBytes = ConnectionRequest::decodeBase64Url(y->mString);
            if (xBytes.size() != ES384_COORDINATE_SIZE || yBytes.size() != ES384_COORDINATE_SIZE)
                return std::string();

            std::string der((const char *) P384_PUBLIC_KEY_PREFIX, sizeof(P384_PUBLIC_KEY_PREFIX));
            der += xBytes;
            der += yBytes;
            return der;
        }

        std::string encode(const std::string &data, const char *alphabet, bool padding) {
            std::string out;
            out.reserve((data.size() + 2) / 3 * 4);

            size_t i = 0;
            while (i + 2 < data.size()) {
                const unsigned value = ((unsigned char) data[i] << 16) | ((unsigned char) data[i + 1] << 8) |
                                       (unsigned char) data[i + 2];
                out.push_back(alphabet[(value >> 18) & 0x3f]);
                out.push_back(alphabet[(value >> 12) & 0x3f]);
                out.push_back(alphabet[(value >> 6) & 0x3f]);
                out.push_back(alphabet[value & 0x3f]);
                i += 3;
            }

            const size_t remaining = data.size() - i;
            if (remaining == 1) {
                const unsigned value = (unsigned char) data[i] << 16;
                out.push_back(alphabet[(value >> 18) & 0x3f]);
                out.push_back(alphabet[(value >> 12) & 0x3f]);
                if (padding) {
                    out.push_back('=');
                    out.push_back('=');
                }
            } else if (remaining == 2) {
                const unsigned value = ((unsigned char) data[i] << 16) | ((unsigned char) data[i + 1] << 8);
                out.push_back(alphabet[(value >> 18) & 0x3f]);
                out.push_back(alphabet[(value >> 12) & 0x3f]);
                out.push_back(alphabet[(value >> 6) & 0x3f]);
                if (padding)
                    out.push_back('=');
            }

            return out;
        }

        std::string encodePublicKey(EVP_PKEY *key) {
            unsigned char *der = nullptr;
            const int length = i2d_PUBKEY(key, &der);
            if (length <= 0 || der == nullptr)
                return std::string();

            const std::string encoded = encode(std::string((const char *) der, (size_t) length), BASE64_ALPHABET, true);
            OPENSSL_free(der);
            return encoded;
        }

        std::string rawSignatureFromDer(const std::string &der) {
            const unsigned char *pointer = (const unsigned char *) der.data();
            ECDSA_SIG *signature = d2i_ECDSA_SIG(nullptr, &pointer, (long) der.size());
            if (signature == nullptr)
                return std::string();

            const BIGNUM *r = nullptr;
            const BIGNUM *s = nullptr;
            ECDSA_SIG_get0(signature, &r, &s);

            std::string raw(ES384_COORDINATE_SIZE * 2, '\0');
            const bool ok = BN_bn2binpad(r, (unsigned char *) &raw[0], (int) ES384_COORDINATE_SIZE) > 0 &&
                            BN_bn2binpad(s, (unsigned char *) &raw[ES384_COORDINATE_SIZE],
                                         (int) ES384_COORDINATE_SIZE) > 0;

            ECDSA_SIG_free(signature);

            if (!ok)
                return std::string();

            return raw;
        }

        std::string derSignatureFromRaw(const std::string &raw) {
            if (raw.size() != ES384_COORDINATE_SIZE * 2)
                return std::string();

            BIGNUM *r = BN_bin2bn((const unsigned char *) raw.data(), (int) ES384_COORDINATE_SIZE, nullptr);
            BIGNUM *s = BN_bin2bn((const unsigned char *) raw.data() + ES384_COORDINATE_SIZE,
                                  (int) ES384_COORDINATE_SIZE, nullptr);
            if (r == nullptr || s == nullptr) {
                BN_free(r);
                BN_free(s);
                return std::string();
            }

            ECDSA_SIG *signature = ECDSA_SIG_new();
            if (signature == nullptr) {
                BN_free(r);
                BN_free(s);
                return std::string();
            }

            ECDSA_SIG_set0(signature, r, s);

            unsigned char *der = nullptr;
            const int length = i2d_ECDSA_SIG(signature, &der);
            std::string result;

            if (length > 0 && der != nullptr) {
                result.assign((const char *) der, (size_t) length);
                OPENSSL_free(der);
            }

            ECDSA_SIG_free(signature);
            return result;
        }

        std::string signWithSha384(EVP_PKEY *key, const std::string &message) {
            EVP_MD_CTX *context = EVP_MD_CTX_new();
            if (context == nullptr)
                return std::string();

            std::string der;
            size_t length = 0;

            if (EVP_DigestSignInit(context, nullptr, EVP_sha384(), nullptr, key) == 1 &&
                EVP_DigestSignUpdate(context, message.data(), message.size()) == 1 &&
                EVP_DigestSignFinal(context, nullptr, &length) == 1) {
                der.resize(length);
                if (EVP_DigestSignFinal(context, (unsigned char *) &der[0], &length) == 1)
                    der.resize(length);
                else
                    der.clear();
            }

            EVP_MD_CTX_free(context);
            return der;
        }

        bool verifyWithSha384(EVP_PKEY *key, const std::string &message, const std::string &derSignature) {
            EVP_MD_CTX *context = EVP_MD_CTX_new();
            if (context == nullptr)
                return false;

            bool ok = false;

            if (EVP_DigestVerifyInit(context, nullptr, EVP_sha384(), nullptr, key) == 1 &&
                EVP_DigestVerifyUpdate(context, message.data(), message.size()) == 1) {
                ok = EVP_DigestVerifyFinal(context, (const unsigned char *) derSignature.data(),
                                           derSignature.size()) == 1;
            }

            EVP_MD_CTX_free(context);
            return ok;
        }

        bool splitCompact(const std::string &token, std::string &header, std::string &payload,
                          std::string &signature) {
            const size_t first = token.find('.');
            if (first == std::string::npos)
                return false;

            const size_t second = token.find('.', first + 1);
            if (second == std::string::npos)
                return false;

            if (token.find('.', second + 1) != std::string::npos)
                return false;

            header = token.substr(0, first);
            payload = token.substr(first + 1, second - first - 1);
            signature = token.substr(second + 1);
            return true;
        }

    }

    Identity::Identity() : mPrivateKey(nullptr) {
    }

    Identity::~Identity() {
        if (mPrivateKey != nullptr)
            EVP_PKEY_free(mPrivateKey);
    }

    std::string Identity::encodeBase64(const std::string &data) {
        return encode(data, BASE64_ALPHABET, true);
    }

    std::string Identity::encodeBase64Url(const std::string &data) {
        return encode(data, BASE64_URL_ALPHABET, false);
    }

    bool Identity::generate(const std::string &domain) {
        if (mPrivateKey != nullptr) {
            EVP_PKEY_free(mPrivateKey);
            mPrivateKey = nullptr;
        }

        EVP_PKEY_CTX *context = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
        if (context == nullptr)
            return false;

        if (EVP_PKEY_keygen_init(context) != 1 ||
            EVP_PKEY_CTX_set_ec_paramgen_curve_nid(context, NID_secp384r1) != 1 ||
            EVP_PKEY_keygen(context, &mPrivateKey) != 1) {
            EVP_PKEY_CTX_free(context);
            return false;
        }

        EVP_PKEY_CTX_free(context);
        mDomain = domain;
        return issueToken();
    }

    bool Identity::issueToken() {
        if (mPrivateKey == nullptr)
            return false;

        const std::string publicKey = encodePublicKey(mPrivateKey);
        if (publicKey.empty())
            return false;

        std::string header = "{\"alg\":\"ES384\",\"x5u\":\"";
        header += publicKey;
        header += "\"}";

        const long long issuedAt = (long long) time(nullptr);

        std::string claims = "{\"exp\":";
        claims += std::to_string(issuedAt + TOKEN_LIFETIME_SECONDS);
        claims += ",\"iat\":";
        claims += std::to_string(issuedAt);
        claims += ",\"cpk\":\"";
        claims += publicKey;
        claims += "\"}";

        const std::string signingInput = encodeBase64Url(header) + "." + encodeBase64Url(claims);
        const std::string der = signWithSha384(mPrivateKey, signingInput);
        if (der.empty())
            return false;

        const std::string raw = rawSignatureFromDer(der);
        if (raw.empty())
            return false;

        std::string token = signingInput + "." + encodeBase64Url(raw);
        std::lock_guard<std::mutex> lock(mTokenMutex);
        mToken = std::move(token);
        return true;
    }

    std::string Identity::getToken() const {
        std::lock_guard<std::mutex> lock(mTokenMutex);
        return mToken;
    }

    std::string Identity::signDetached(const std::string &payload) const {
        if (mPrivateKey == nullptr)
            return std::string();

        const std::string header = encodeBase64Url("{\"alg\":\"ES384\"}");
        const std::string signingInput = header + "." + encodeBase64Url(payload);

        const std::string der = signWithSha384(mPrivateKey, signingInput);
        if (der.empty())
            return std::string();

        const std::string raw = rawSignatureFromDer(der);
        if (raw.empty())
            return std::string();

        return header + ".." + encodeBase64Url(raw);
    }

    bool Identity::verifyDetached(EVP_PKEY *key, const std::string &detached, const std::string &payload) {
        if (key == nullptr)
            return false;

        std::string header;
        std::string embedded;
        std::string signature;
        if (!splitCompact(detached, header, embedded, signature))
            return false;

        if (!embedded.empty())
            return false;

        const std::string signingInput = header + "." + encodeBase64Url(payload);
        const std::string raw = ConnectionRequest::decodeBase64Url(signature);
        const std::string der = derSignatureFromRaw(raw);
        if (der.empty())
            return false;

        return verifyWithSha384(key, signingInput, der);
    }

    EVP_PKEY *Identity::claimPublicKey(const std::string &token, std::string &failure) {
        std::string header;
        std::string payload;
        std::string signature;

        if (!splitCompact(token, header, payload, signature)) {
            failure = "token is not a compact JWS";
            return nullptr;
        }

        const std::string claims = ConnectionRequest::decodeBase64Url(payload);
        const std::unique_ptr<json::Value> parsed = json::parse(claims);
        if (parsed == nullptr || !parsed->isObject()) {
            failure = "token claims are not a JSON object";
            return nullptr;
        }

        const json::Value *cpk = parsed->get("cpk");
        if (cpk == nullptr) {
            failure = "cpk claim is missing";
            return nullptr;
        }

        if (!cpk->isString() && !cpk->isObject()) {
            failure = "cpk claim is neither a base64 encoded PKIX public key nor a JSON web key";
            return nullptr;
        }

        const json::Value *expiry = parsed->get("exp");
        if (expiry != nullptr && expiry->isNumber()) {
            const long long now = (long long) time(nullptr);
            if ((long long) expiry->number() < now) {
                failure = "token has expired";
                return nullptr;
            }
        }

        const std::string der = cpk->isObject() ? publicKeyFromJsonWebKey(*cpk)
                                                : ConnectionRequest::decodeBase64Url(cpk->mString);
        if (der.empty()) {
            failure = "cpk claim could not be decoded";
            return nullptr;
        }

        const unsigned char *pointer = (const unsigned char *) der.data();
        EVP_PKEY *key = d2i_PUBKEY(nullptr, &pointer, (long) der.size());
        if (key == nullptr) {
            failure = "cpk claim is not a valid public key";
            return nullptr;
        }

        return key;
    }
}
