#pragma once

#include <cstdint>
#include <memory>
#include <string>

struct evp_pkey_st;

namespace acme {

    // P-256 key: account key (ES256) and certificate key
    class EcKey {
    public:
        static std::shared_ptr<EcKey> generate();

        static std::shared_ptr<EcKey> fromPem(const std::string &pem);

        ~EcKey();

        EcKey(const EcKey &) = delete;

        EcKey &operator=(const EcKey &) = delete;

        std::string toPem() const;

        // RFC 7638 member order: crv, kty, x, y
        std::string jwk() const;

        std::string thumbprint() const;

        // raw r || s, 64 bytes
        std::string signEs256(const std::string &data) const;

        bool verifyEs256(const std::string &data, const std::string &signature) const;

        evp_pkey_st *get() const {
            return mKey;
        }

    private:
        explicit EcKey(evp_pkey_st *key);

        evp_pkey_st *mKey;
    };

    std::string sha256(const std::string &data);

    // lowercase, labels of 1 to 63 letters, digits or inner hyphens, at least two labels, 253 at most, no IP address
    bool isDomain(const std::string &domain);

    std::string normalizeDomain(const std::string &domain);

    // flattened JWS: jwk before the account exists, kid after
    std::string signJws(const EcKey &key, const std::string &url, const std::string &nonce, const std::string &kid,
                        const std::string &payload);

    std::string keyAuthorization(const std::string &token, const EcKey &accountKey);

    // DER CSR with the domain as subject and only SAN
    std::string createCsr(const EcKey &certificateKey, const std::string &domain);

    // RFC 8737 challenge certificate: SAN domain, critical id-pe-acmeIdentifier over sha256(key authorization)
    bool createTlsAlpnCertificate(const std::string &domain, const std::string &keyAuthorization,
                                  std::string &outCertificatePem, std::string &outKeyPem);

    // notBefore and notAfter of the first certificate in a PEM chain, unix seconds
    bool readValidity(const std::string &chainPem, int64_t &outNotBefore, int64_t &outNotAfter);

    bool certificateCovers(const std::string &chainPem, const std::string &domain);

    bool keyMatches(const std::string &chainPem, const EcKey &key);

    // a third of the lifetime, at most 30 days, before notAfter
    int64_t renewalTime(int64_t notBefore, int64_t notAfter);

}
