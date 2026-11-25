#include "Network/Crypto/KeyPair.h"

#include "Network/Crypto/Base64.h"

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/x509.h>

#include <vector>

namespace {

    const size_t COORDINATE_SIZE = 48;

    std::string encodePublicKey(EVP_PKEY *key) {
        const int length = i2d_PUBKEY(key, nullptr);
        if (length <= 0)
            return {};

        std::string der((size_t) length, '\0');
        unsigned char *cursor = (unsigned char *) &der[0];
        if (i2d_PUBKEY(key, &cursor) != length)
            return {};

        return Base64::encode(der);
    }

}

KeyPair::KeyPair(EVP_PKEY *key, bool hasPrivateKey)
        : mKey(key), mHasPrivateKey(hasPrivateKey), mPublicKeyBase64(encodePublicKey(key)) {
}

KeyPair::~KeyPair() {
    EVP_PKEY_free(mKey);
}

std::shared_ptr<KeyPair> KeyPair::generate() {
    EVP_PKEY_CTX *context = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, nullptr);
    if (context == nullptr)
        return nullptr;

    EVP_PKEY *key = nullptr;
    const bool generated = EVP_PKEY_keygen_init(context) == 1
                           && EVP_PKEY_CTX_set_ec_paramgen_curve_nid(context, NID_secp384r1) == 1
                           && EVP_PKEY_keygen(context, &key) == 1;
    EVP_PKEY_CTX_free(context);

    if (!generated) {
        EVP_PKEY_free(key);
        return nullptr;
    }

    return std::shared_ptr<KeyPair>(new KeyPair(key, true));
}

std::shared_ptr<KeyPair> KeyPair::fromPublicKey(const std::string &publicKeyBase64) {
    std::string der;
    if (!Base64::decode(publicKeyBase64, der) || der.empty())
        return nullptr;

    const unsigned char *cursor = (const unsigned char *) der.data();
    EVP_PKEY *key = d2i_PUBKEY(nullptr, &cursor, (long) der.size());
    if (key == nullptr)
        return nullptr;

    if (EVP_PKEY_base_id(key) != EVP_PKEY_EC) {
        EVP_PKEY_free(key);
        return nullptr;
    }

    return std::shared_ptr<KeyPair>(new KeyPair(key, false));
}

std::string KeyPair::deriveSharedSecret(const KeyPair &remote) const {
    if (!mHasPrivateKey)
        return {};

    EVP_PKEY_CTX *context = EVP_PKEY_CTX_new(mKey, nullptr);
    if (context == nullptr)
        return {};

    std::string secret;
    size_t length = 0;
    if (EVP_PKEY_derive_init(context) == 1 && EVP_PKEY_derive_set_peer(context, remote.mKey) == 1
        && EVP_PKEY_derive(context, nullptr, &length) == 1) {
        secret.resize(length);
        if (EVP_PKEY_derive(context, (unsigned char *) &secret[0], &length) == 1)
            secret.resize(length);
        else
            secret.clear();
    }

    EVP_PKEY_CTX_free(context);
    return secret;
}

std::string KeyPair::sign(const std::string &data) const {
    if (!mHasPrivateKey)
        return {};

    EVP_MD_CTX *context = EVP_MD_CTX_new();
    if (context == nullptr)
        return {};

    std::vector<unsigned char> der;
    size_t length = 0;
    bool signedData = EVP_DigestSignInit(context, nullptr, EVP_sha384(), nullptr, mKey) == 1
                      && EVP_DigestSign(context, nullptr, &length, (const unsigned char *) data.data(),
                                        data.size()) == 1;
    if (signedData) {
        der.resize(length);
        signedData = EVP_DigestSign(context, der.data(), &length, (const unsigned char *) data.data(),
                                    data.size()) == 1;
        der.resize(length);
    }
    EVP_MD_CTX_free(context);

    if (!signedData)
        return {};

    const unsigned char *cursor = der.data();
    ECDSA_SIG *signature = d2i_ECDSA_SIG(nullptr, &cursor, (long) der.size());
    if (signature == nullptr)
        return {};

    const BIGNUM *r = nullptr;
    const BIGNUM *s = nullptr;
    ECDSA_SIG_get0(signature, &r, &s);

    std::string raw(COORDINATE_SIZE * 2, '\0');
    const bool encoded = BN_bn2binpad(r, (unsigned char *) &raw[0], (int) COORDINATE_SIZE) == (int) COORDINATE_SIZE
                         && BN_bn2binpad(s, (unsigned char *) &raw[COORDINATE_SIZE], (int) COORDINATE_SIZE)
                            == (int) COORDINATE_SIZE;
    ECDSA_SIG_free(signature);

    return encoded ? raw : std::string();
}

bool KeyPair::verify(const std::string &data, const std::string &signature) const {
    if (signature.size() != COORDINATE_SIZE * 2)
        return false;

    ECDSA_SIG *ecdsa = ECDSA_SIG_new();
    if (ecdsa == nullptr)
        return false;

    BIGNUM *r = BN_bin2bn((const unsigned char *) signature.data(), (int) COORDINATE_SIZE, nullptr);
    BIGNUM *s = BN_bin2bn((const unsigned char *) signature.data() + COORDINATE_SIZE, (int) COORDINATE_SIZE,
                          nullptr);
    if (r == nullptr || s == nullptr || ECDSA_SIG_set0(ecdsa, r, s) != 1) {
        BN_free(r);
        BN_free(s);
        ECDSA_SIG_free(ecdsa);
        return false;
    }

    const int derLength = i2d_ECDSA_SIG(ecdsa, nullptr);
    std::vector<unsigned char> der(derLength > 0 ? (size_t) derLength : 0);
    unsigned char *cursor = der.data();
    const bool encoded = derLength > 0 && i2d_ECDSA_SIG(ecdsa, &cursor) == derLength;
    ECDSA_SIG_free(ecdsa);
    if (!encoded)
        return false;

    EVP_MD_CTX *context = EVP_MD_CTX_new();
    if (context == nullptr)
        return false;

    const bool verified = EVP_DigestVerifyInit(context, nullptr, EVP_sha384(), nullptr, mKey) == 1
                          && EVP_DigestVerify(context, der.data(), der.size(), (const unsigned char *) data.data(),
                                              data.size()) == 1;
    EVP_MD_CTX_free(context);
    return verified;
}
