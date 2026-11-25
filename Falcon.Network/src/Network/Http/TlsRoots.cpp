#include "Network/Http/TlsRoots.h"

#ifdef _WIN32

#include <windows.h>
#include <wincrypt.h>

#undef X509_NAME
#undef X509_EXTENSIONS
#undef PKCS7_SIGNER_INFO
#undef OCSP_REQUEST
#undef OCSP_RESPONSE

#endif

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#ifdef __ANDROID__
#include <dirent.h>
#include <string>
#endif

#include <mutex>
#include <vector>

namespace {

#ifdef __ANDROID__
    const char *const ANDROID_SYSTEM_CERTIFICATES = "/system/etc/security/cacerts";
    const char *const ANDROID_APEX_CERTIFICATES = "/apex/com.android.conscrypt/cacerts";

    void readDirectory(const char *directory, std::vector<X509 *> &outCertificates) {
        DIR *handle = opendir(directory);
        if (handle == nullptr)
            return;

        while (dirent *entry = readdir(handle)) {
            if (entry->d_name[0] == '.')
                continue;

            const std::string path = std::string(directory) + "/" + entry->d_name;
            BIO *file = BIO_new_file(path.c_str(), "r");
            if (file == nullptr)
                continue;

            X509 *certificate = PEM_read_bio_X509(file, nullptr, nullptr, nullptr);
            if (certificate != nullptr)
                outCertificates.push_back(certificate);
            BIO_free(file);
        }

        closedir(handle);
    }
#endif

    std::vector<X509 *> readSystemCertificates() {
        std::vector<X509 *> certificates;

#ifdef _WIN32
        HCERTSTORE store = CertOpenSystemStoreW(0, L"ROOT");
        if (store != nullptr) {
            PCCERT_CONTEXT entry = nullptr;
            while ((entry = CertEnumCertificatesInStore(store, entry)) != nullptr) {
                const unsigned char *encoded = entry->pbCertEncoded;
                X509 *certificate = d2i_X509(nullptr, &encoded, (long) entry->cbCertEncoded);
                if (certificate != nullptr)
                    certificates.push_back(certificate);
            }
            CertCloseStore(store, 0);
        }
#endif

#ifdef __ANDROID__
        readDirectory(ANDROID_APEX_CERTIFICATES, certificates);
        if (certificates.empty())
            readDirectory(ANDROID_SYSTEM_CERTIFICATES, certificates);
#endif

        ERR_clear_error();
        return certificates;
    }

    const std::vector<X509 *> &systemCertificates() {
        static std::once_flag once;
        static std::vector<X509 *> certificates;
        std::call_once(once, [] {
            certificates = readSystemCertificates();
        });
        return certificates;
    }

}

namespace TlsRoots {

    void load(ssl_ctx_st *context) {
        SSL_CTX_set_default_verify_paths(context);

        X509_STORE *store = SSL_CTX_get_cert_store(context);
        for (X509 *certificate: systemCertificates())
            X509_STORE_add_cert(store, certificate);
        ERR_clear_error();
    }

}
