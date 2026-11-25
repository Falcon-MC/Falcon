#pragma once

#include <mutex>
#include <string>

typedef struct evp_pkey_st EVP_PKEY;

namespace nethernet {
    class Identity {
    public:
        Identity();

        ~Identity();

        bool generate(const std::string &domain);

        bool issueToken();

        // signaling threads negotiate concurrently, so the token is copied under the lock that guards its reissue
        std::string getToken() const;

        const std::string &getDomain() const { return mDomain; }

        std::string signDetached(const std::string &payload) const;

        static bool verifyDetached(EVP_PKEY *key, const std::string &detached, const std::string &payload);

        static EVP_PKEY *claimPublicKey(const std::string &token, std::string &failure);

        static std::string encodeBase64(const std::string &data);

        static std::string encodeBase64Url(const std::string &data);

    private:
        EVP_PKEY *mPrivateKey;
        mutable std::mutex mTokenMutex;
        std::string mToken;
        std::string mDomain;
    };
}
