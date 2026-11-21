#include "Network/TransportTranscript.h"

#include <openssl/evp.h>
#include <openssl/ssl.h>

#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
    struct Transcript {
        std::vector<unsigned char> mRecords;
        std::string mFingerprint;
    };

    // type, length, sequence, fragment offset, fragment length
    const size_t HEADER_LENGTH = 12;
    const size_t MAX_RECORD = 4096;
    const size_t MAX_TRANSCRIPT = 16 * 1024;
    const size_t MAX_SESSIONS = 2048;

    const unsigned char CLIENT_HELLO = 1;
    const unsigned char CERTIFICATE = 11;

    std::mutex gMutex;
    std::unordered_map<const void *, Transcript> gSessions;

    std::string fingerprintOf(const unsigned char *certificate, size_t size) {
        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned int length = 0;
        if (EVP_Digest(certificate, size, digest, &length, EVP_sha256(), nullptr) != 1)
            return std::string();

        static const char HEX[] = "0123456789ABCDEF";
        std::string fingerprint;
        for (unsigned int index = 0; index < length; index++) {
            if (index != 0)
                fingerprint.push_back(':');
            fingerprint.push_back(HEX[digest[index] >> 4]);
            fingerprint.push_back(HEX[digest[index] & 15]);
        }
        return fingerprint;
    }

    bool sameFingerprint(const std::string &left, const char *right) {
        const size_t length = std::strlen(right);
        if (left.size() != length)
            return false;
        for (size_t index = 0; index < length; index++) {
            char a = left[index];
            char b = right[index];
            if (a >= 'a' && a <= 'f')
                a = (char) (a - 32);
            if (b >= 'a' && b <= 'f')
                b = (char) (b - 32);
            if (a != b)
                return false;
        }
        return true;
    }

    void record(const void *session, bool sent, const unsigned char *data, size_t length) {
        if (length < HEADER_LENGTH)
            return;

        std::lock_guard<std::mutex> lock(gMutex);
        // a first hello opens a new session on a pointer that may have served another
        if (sent && data[0] == CLIENT_HELLO && data[4] == 0 && data[5] == 0) {
            if (gSessions.size() >= MAX_SESSIONS)
                gSessions.clear();
            gSessions[session] = Transcript();
        }

        const auto found = gSessions.find(session);
        if (found == gSessions.end())
            return;
        Transcript &transcript = found->second;

        // of what the server sent itself, the header says enough
        const size_t kept = sent ? HEADER_LENGTH : length;
        if (kept > MAX_RECORD || transcript.mRecords.size() + 3 + kept > MAX_TRANSCRIPT)
            return;

        transcript.mRecords.push_back(sent ? 1 : 0);
        transcript.mRecords.push_back((unsigned char) (kept >> 8));
        transcript.mRecords.push_back((unsigned char) (kept & 255));
        transcript.mRecords.insert(transcript.mRecords.end(), data, data + kept);

        // a certificate list: its length, then the first certificate's length and bytes
        if (!sent && data[0] == CERTIFICATE && length >= HEADER_LENGTH + 6) {
            const unsigned char *body = data + HEADER_LENGTH;
            const size_t size = ((size_t) body[3] << 16) | ((size_t) body[4] << 8) | body[5];
            if (size != 0 && HEADER_LENGTH + 6 + size <= length)
                transcript.mFingerprint = fingerprintOf(body + 6, size);
        }
    }

    void onMessage(int sent, int version, int contentType, const void *data, size_t length, SSL *session,
                   void *argument) {
        (void) version;
        (void) argument;
        if (contentType != SSL3_RT_HANDSHAKE || data == nullptr || session == nullptr || SSL_is_dtls(session) != 1)
            return;
        record(session, sent != 0, static_cast<const unsigned char *>(data), length);
    }
}

extern "C" {

// the transport library is built to call this where it would set its info callback: its contexts are
// not reachable any other way
void falcon_transport_install(SSL_CTX *context, void (*callback)(const SSL *, int, int)) {
    SSL_CTX_set_info_callback(context, callback);
    SSL_CTX_set_msg_callback(context, &onMessage);
}

size_t falcon_transport_take(const char *fingerprint, unsigned char *out, size_t capacity) {
    if (fingerprint == nullptr || out == nullptr)
        return 0;

    std::lock_guard<std::mutex> lock(gMutex);
    for (auto it = gSessions.begin(); it != gSessions.end(); ++it) {
        if (!sameFingerprint(it->second.mFingerprint, fingerprint))
            continue;

        const size_t size = it->second.mRecords.size();
        const bool fits = size <= capacity;
        if (fits)
            std::memcpy(out, it->second.mRecords.data(), size);
        gSessions.erase(it);
        return fits ? size : 0;
    }
    return 0;
}
}
