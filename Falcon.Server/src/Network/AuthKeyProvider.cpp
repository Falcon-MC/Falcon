#include "Network/AuthKeyProvider.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <wincrypt.h>
#undef X509_NAME
#undef X509_EXTENSIONS
#undef PKCS7_ISSUER_AND_SERIAL
#undef PKCS7_SIGNER_INFO
#undef OCSP_REQUEST
#undef OCSP_RESPONSE
#else
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif

#include "Core/Debug/BedrockLog.h"
#include "Core/Net/HttpsClient.h"
#include "Network/ConnectionRequest.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/evp.h>

#ifdef _WIN32
typedef SOCKET FalconSocket;
#define FALCON_INVALID_SOCKET INVALID_SOCKET
#define FALCON_CLOSE_SOCKET ::closesocket
#else
typedef int FalconSocket;
#define FALCON_INVALID_SOCKET (-1)
#define FALCON_CLOSE_SOCKET ::close
#endif

namespace {

    const long long KEY_REFRESH_INTERVAL_SECONDS = 30 * 60;
    const long long FAILURE_BACKOFF_SECONDS = 60;

    const char *MINECRAFT_VERSION_NETWORK = "1.26.60";
    const char *DISCOVERY_URL_PREFIX =
            "https://client.discovery.minecraft-services.net/api/v1.0/discovery/MinecraftPE/builds/";
    const char *AUTHORIZATION_SERVICE_URI_FALLBACK = "https://authorization.franchise.minecraft-services.net";
    const char *OPENID_CONFIGURATION_PATH = "/.well-known/openid-configuration";
    const char *KEYS_PATH = "/.well-known/keys";

    long long currentTime() {
        return (long long) std::time(nullptr);
    }

    std::string findJsonObject(const std::string &json, const std::string &key) {
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

    std::vector<std::string> findJsonArrayObjects(const std::string &json, const std::string &key) {
        std::vector<std::string> objects;

        const std::string pattern = "\"" + key + "\"";
        size_t position = json.find(pattern);
        if (position == std::string::npos)
            return objects;

        position = json.find('[', position + pattern.size());
        if (position == std::string::npos)
            return objects;

        int arrayDepth = 0;
        int objectDepth = 0;
        size_t objectStart = std::string::npos;

        for (size_t i = position; i < json.size(); i++) {
            const char c = json[i];

            if (c == '[') {
                arrayDepth++;
                continue;
            }

            if (c == ']') {
                arrayDepth--;
                if (arrayDepth == 0)
                    break;
                continue;
            }

            if (c == '{') {
                if (objectDepth == 0)
                    objectStart = i;
                objectDepth++;
                continue;
            }

            if (c == '}') {
                objectDepth--;
                if (objectDepth == 0 && objectStart != std::string::npos) {
                    objects.push_back(json.substr(objectStart, i - objectStart + 1));
                    objectStart = std::string::npos;
                }
            }
        }

        return objects;
    }

    std::string encodeDerLength(size_t length) {
        std::string out;

        if (length <= 0x7f) {
            out.push_back((char) length);
            return out;
        }

        std::string bytes;
        size_t value = length;
        while (value > 0) {
            bytes.insert(bytes.begin(), (char) (value & 0xff));
            value >>= 8;
        }

        out.push_back((char) (0x80 | bytes.size()));
        out.append(bytes);
        return out;
    }

    std::string encodeDerBytes(unsigned char tag, const std::string &data) {
        std::string out;
        out.push_back((char) tag);
        out.append(encodeDerLength(data.size()));
        out.append(data);
        return out;
    }

    std::string encodeDerInteger(const std::string &value) {
        size_t start = 0;
        while (start + 1 < value.size() && value[start] == 0)
            start++;

        std::string trimmed = value.substr(start);
        if (trimmed.empty())
            trimmed.push_back((char) 0);

        if ((unsigned char) trimmed[0] >= 0x80)
            trimmed.insert(trimmed.begin(), (char) 0);

        return encodeDerBytes(0x02, trimmed);
    }

    EVP_PKEY *rsaPublicKeyFromModExp(const std::string &modulusBase64, const std::string &exponentBase64) {
        const std::string modulus = ConnectionRequest::decodeBase64Url(modulusBase64);
        const std::string exponent = ConnectionRequest::decodeBase64Url(exponentBase64);

        if (modulus.empty() || exponent.empty())
            return nullptr;

        const std::string sequence = encodeDerBytes(0x30, encodeDerInteger(modulus) + encodeDerInteger(exponent));

        std::string bitString;
        bitString.push_back((char) 0);
        bitString.append(sequence);

        const unsigned char rsaIdentifier[] = {
                0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
                0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00
        };

        std::string algorithm(reinterpret_cast<const char *>(rsaIdentifier), sizeof(rsaIdentifier));
        const std::string der = encodeDerBytes(0x30, algorithm + encodeDerBytes(0x03, bitString));

        const unsigned char *pointer = reinterpret_cast<const unsigned char *>(der.data());
        return d2i_PUBKEY(nullptr, &pointer, (long) der.size());
    }

    std::string resolveAuthServiceUri() {
        std::string body;
        const std::string url = std::string(DISCOVERY_URL_PREFIX) + MINECRAFT_VERSION_NETWORK;

        if (!HttpsClient::get(url, body))
            return std::string();

        const std::string result = findJsonObject(body, "result");
        if (result.empty())
            return std::string();

        const std::string environments = findJsonObject(result, "serviceEnvironments");
        if (environments.empty())
            return std::string();

        const std::string auth = findJsonObject(environments, "auth");
        if (auth.empty())
            return std::string();

        const std::string prod = findJsonObject(auth, "prod");
        if (prod.empty())
            return std::string();

        return ConnectionRequest::findJsonString(prod, "serviceUri");
    }

    bool resolveOpenIdConfiguration(const std::string &authServiceUri, std::string &issuer, std::string &jwksUri) {
        std::string body;
        if (!HttpsClient::get(authServiceUri + OPENID_CONFIGURATION_PATH, body))
            return false;

        issuer = ConnectionRequest::findJsonString(body, "issuer");
        jwksUri = ConnectionRequest::findJsonString(body, "jwks_uri");
        return !issuer.empty() && !jwksUri.empty();
    }

}

AuthKeyProvider &AuthKeyProvider::getInstance() {
    static AuthKeyProvider instance;
    return instance;
}

AuthKeyProvider::~AuthKeyProvider() {
    stop();
    _clear();
}

void AuthKeyProvider::_clear() {
    for (std::map<std::string, EVP_PKEY *>::iterator it = mKeys.begin(); it != mKeys.end(); ++it)
        EVP_PKEY_free(it->second);

    mKeys.clear();
}

bool AuthKeyProvider::_fetch(std::map<std::string, EVP_PKEY *> &outKeys, std::string &outIssuer) {
    LOG_TRACE(LogAreaID::Network, "Fetching authentication keys from the Minecraft services");

    std::string authServiceUri = resolveAuthServiceUri();
    if (authServiceUri.empty()) {
        LOG_WARN(LogAreaID::Network, "Service discovery failed, falling back to the well known authorization service");
        authServiceUri = AUTHORIZATION_SERVICE_URI_FALLBACK;
    }

    std::string issuer;
    std::string jwksUri;

    if (!resolveOpenIdConfiguration(authServiceUri, issuer, jwksUri)) {
        issuer = authServiceUri;
        jwksUri = authServiceUri + KEYS_PATH;
    }

    std::string body;
    if (!HttpsClient::get(jwksUri, body)) {
        LOG_ERROR(LogAreaID::Network,
                  "Failed to fetch authentication keys, Xbox players will not be able to authenticate");
        return false;
    }

    std::map<std::string, EVP_PKEY *> keys;

    const std::vector<std::string> entries = findJsonArrayObjects(body, "keys");
    for (size_t i = 0; i < entries.size(); i++) {
        const std::string &entry = entries[i];

        const std::string kty = ConnectionRequest::findJsonString(entry, "kty");
        const std::string use = ConnectionRequest::findJsonString(entry, "use");
        const std::string kid = ConnectionRequest::findJsonString(entry, "kid");

        if (kid.empty())
            continue;

        if (kty != "RSA" || use != "sig") {
            LOG_WARN(LogAreaID::Network, "Authentication key %s is not a signing rsa key, skipping it", kid.c_str());
            continue;
        }

        EVP_PKEY *key = rsaPublicKeyFromModExp(ConnectionRequest::findJsonString(entry, "n"),
                                               ConnectionRequest::findJsonString(entry, "e"));
        if (key == nullptr) {
            LOG_WARN(LogAreaID::Network, "Failed to parse authentication key %s", kid.c_str());
            continue;
        }

        keys.insert(std::make_pair(kid, key));
    }

    if (keys.empty()) {
        LOG_ERROR(LogAreaID::Network, "No usable authentication key was returned by the Minecraft services");
        return false;
    }

    outKeys = keys;
    outIssuer = issuer;

    LOG_TRACE(LogAreaID::Network, "Loaded %u authentication keys from issuer %s", (unsigned) keys.size(),
              issuer.c_str());
    LOG_INFO(LogAreaID::Network, "Signed in to signaling service successfully");
    return true;
}

void AuthKeyProvider::_worker() {
    for (;;) {
        std::unique_lock<std::mutex> guard(mMutex);

        mCondition.wait(guard, [this]() { return !mRunning || mRefreshRequested; });

        if (!mRunning)
            return;

        mRefreshRequested = false;
        guard.unlock();

        std::map<std::string, EVP_PKEY *> keys;
        std::string issuer;
        const bool ok = _fetch(keys, issuer);

        guard.lock();

        if (!mRunning) {
            for (std::map<std::string, EVP_PKEY *>::iterator it = keys.begin(); it != keys.end(); ++it)
                EVP_PKEY_free(it->second);
            return;
        }

        if (ok) {
            _clear();
            mKeys = keys;
            mIssuer = issuer;
            mLastFetch = currentTime();
        } else {
            mLastFailure = currentTime();
        }
    }
}

void AuthKeyProvider::_requestRefresh() {
    if (mRefreshRequested)
        return;

    mRefreshRequested = true;
    mCondition.notify_one();
}

void AuthKeyProvider::start() {
    std::lock_guard<std::mutex> guard(mMutex);

    if (mRunning)
        return;

    mRunning = true;
    mWorker = std::thread(&AuthKeyProvider::_worker, this);
    _requestRefresh();
}

void AuthKeyProvider::stop() {
    {
        std::lock_guard<std::mutex> guard(mMutex);

        if (!mRunning)
            return;

        mRunning = false;
        mCondition.notify_one();
    }

    if (mWorker.joinable())
        mWorker.join();
}

EVP_PKEY *AuthKeyProvider::acquireKey(const std::string &keyId, std::string &issuer) {
    std::lock_guard<std::mutex> guard(mMutex);

    const long long now = currentTime();
    const bool known = mKeys.find(keyId) != mKeys.end();
    const bool stale = mLastFetch < now - KEY_REFRESH_INTERVAL_SECONDS;

    if ((mKeys.empty() || (!known && stale)) && mLastFailure < now - FAILURE_BACKOFF_SECONDS)
        _requestRefresh();

    std::map<std::string, EVP_PKEY *>::iterator it = mKeys.find(keyId);
    if (it == mKeys.end())
        return nullptr;

    issuer = mIssuer;
    EVP_PKEY_up_ref(it->second);
    return it->second;
}
