#include "Network/Acme/AcmeChallengeStore.h"

#include <cctype>

namespace acme {

    namespace {
        // SNI and DNS names compare without case
        std::string lowered(const std::string &value) {
            std::string result = value;
            for (char &character: result)
                character = (char) std::tolower((unsigned char) character);
            return result;
        }
    }

    void ChallengeStore::putHttp(const std::string &token, const std::string &keyAuthorization) {
        std::lock_guard<std::mutex> lock(mMutex);
        mHttp[token] = keyAuthorization;
    }

    void ChallengeStore::removeHttp(const std::string &token) {
        std::lock_guard<std::mutex> lock(mMutex);
        mHttp.erase(token);
    }

    bool ChallengeStore::findHttp(const std::string &token, std::string &outKeyAuthorization) const {
        std::lock_guard<std::mutex> lock(mMutex);
        const auto it = mHttp.find(token);
        if (it == mHttp.end())
            return false;

        outKeyAuthorization = it->second;
        return true;
    }

    void ChallengeStore::putTlsAlpn(const std::string &domain, const std::string &certificatePem,
                                    const std::string &keyPem) {
        std::lock_guard<std::mutex> lock(mMutex);
        TlsAlpnAnswer &answer = mTlsAlpn[lowered(domain)];
        answer.mCertificatePem = certificatePem;
        answer.mKeyPem = keyPem;
    }

    void ChallengeStore::removeTlsAlpn(const std::string &domain) {
        std::lock_guard<std::mutex> lock(mMutex);
        mTlsAlpn.erase(lowered(domain));
    }

    bool ChallengeStore::findTlsAlpn(const std::string &domain, std::string &outCertificatePem,
                                     std::string &outKeyPem) const {
        std::lock_guard<std::mutex> lock(mMutex);
        const auto it = mTlsAlpn.find(lowered(domain));
        if (it == mTlsAlpn.end())
            return false;

        outCertificatePem = it->second.mCertificatePem;
        outKeyPem = it->second.mKeyPem;
        return true;
    }

    bool ChallengeStore::empty() const {
        std::lock_guard<std::mutex> lock(mMutex);
        return mHttp.empty() && mTlsAlpn.empty();
    }

}
