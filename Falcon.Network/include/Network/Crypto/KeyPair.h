#pragma once

#include <memory>
#include <string>

struct evp_pkey_st;

class KeyPair {
public:
    static std::shared_ptr<KeyPair> generate();

    static std::shared_ptr<KeyPair> fromPublicKey(const std::string &publicKeyBase64);

    ~KeyPair();

    KeyPair(const KeyPair &) = delete;

    KeyPair &operator=(const KeyPair &) = delete;

    bool hasPrivateKey() const {
        return mHasPrivateKey;
    }

    const std::string &getPublicKeyBase64() const {
        return mPublicKeyBase64;
    }

    std::string deriveSharedSecret(const KeyPair &remote) const;

    std::string sign(const std::string &data) const;

    bool verify(const std::string &data, const std::string &signature) const;

private:
    KeyPair(evp_pkey_st *key, bool hasPrivateKey);

    evp_pkey_st *mKey;
    bool mHasPrivateKey;
    std::string mPublicKeyBase64;
};
