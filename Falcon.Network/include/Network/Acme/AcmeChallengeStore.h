#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

namespace acme {

    // pending answers, written by the issuing thread and read by every signaling client thread
    class ChallengeStore {
    public:
        void putHttp(const std::string &token, const std::string &keyAuthorization);

        void removeHttp(const std::string &token);

        bool findHttp(const std::string &token, std::string &outKeyAuthorization) const;

        void putTlsAlpn(const std::string &domain, const std::string &certificatePem, const std::string &keyPem);

        void removeTlsAlpn(const std::string &domain);

        bool findTlsAlpn(const std::string &domain, std::string &outCertificatePem, std::string &outKeyPem) const;

        bool empty() const;

    private:
        struct TlsAlpnAnswer {
            std::string mCertificatePem;
            std::string mKeyPem;
        };

        mutable std::mutex mMutex;
        std::unordered_map<std::string, std::string> mHttp;
        std::unordered_map<std::string, TlsAlpnAnswer> mTlsAlpn;
    };

}
