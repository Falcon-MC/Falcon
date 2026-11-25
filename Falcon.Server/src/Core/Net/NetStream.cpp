#include "Core/Net/NetStream.h"

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
#include <netinet/in.h>
#include <sys/socket.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#endif

#include <algorithm>
#include <climits>
#include <cstring>

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#ifdef _WIN32
typedef SOCKET NativeSocket;
#define NET_INVALID_SOCKET INVALID_SOCKET
#define NET_CLOSE_SOCKET ::closesocket
#else
typedef int NativeSocket;
#define NET_INVALID_SOCKET (-1)
#define NET_CLOSE_SOCKET ::close
#endif

namespace {
    void initialiseSockets() {
#ifdef _WIN32
        static bool started = false;
        if (started)
            return;
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
        started = true;
#endif
    }

    int lastSocketError() {
#ifdef _WIN32
        return WSAGetLastError();
#else
        return errno;
#endif
    }

    bool wouldBlock(int code) {
#ifdef _WIN32
        return code == WSAEWOULDBLOCK;
#else
        return code == EAGAIN || code == EWOULDBLOCK;
#endif
    }

    int timedOutError() {
#ifdef _WIN32
        return WSAETIMEDOUT;
#else
        return ETIMEDOUT;
#endif
    }

    // >0 ready, 0 the time ran out, <0 an error; milliseconds -1 waits forever
    int waitSocket(NativeSocket descriptor, bool write, int milliseconds) {
#ifdef _WIN32
        WSAPOLLFD entry{};
        entry.fd = descriptor;
        entry.events = write ? POLLWRNORM : POLLRDNORM;
        return WSAPoll(&entry, 1, milliseconds);
#else
        struct pollfd entry{};
        entry.fd = descriptor;
        entry.events = write ? POLLOUT : POLLIN;
        return ::poll(&entry, 1, milliseconds);
#endif
    }

    std::string socketErrorText(int code) {
#ifdef _WIN32
        char buffer[256] = {0};
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, (DWORD) code, 0, buffer,
                       sizeof(buffer), nullptr);
        std::string text = buffer;
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' '))
            text.pop_back();
        return text;
#else
        return std::strerror(code);
#endif
    }

    void setBlocking(NativeSocket descriptor, bool blocking) {
#ifdef _WIN32
        u_long mode = blocking ? 0 : 1;
        ioctlsocket(descriptor, FIONBIO, &mode);
#else
        int flags = fcntl(descriptor, F_GETFL, 0);
        if (flags < 0)
            return;
        fcntl(descriptor, F_SETFL, blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK));
#endif
    }

    void setNoSigpipe(NativeSocket descriptor) {
#ifdef SO_NOSIGPIPE
        int one = 1;
        setsockopt(descriptor, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#else
        (void) descriptor;
#endif
    }

    void loadSystemTrustStore(SSL_CTX *context) {
#ifdef _WIN32
        HCERTSTORE store = CertOpenSystemStoreA(0, "ROOT");
        if (store == nullptr)
            return;
        X509_STORE *target = SSL_CTX_get_cert_store(context);
        PCCERT_CONTEXT entry = nullptr;
        while ((entry = CertEnumCertificatesInStore(store, entry)) != nullptr) {
            const unsigned char *pointer = entry->pbCertEncoded;
            X509 *certificate = d2i_X509(nullptr, &pointer, (long) entry->cbCertEncoded);
            if (certificate != nullptr) {
                X509_STORE_add_cert(target, certificate);
                X509_free(certificate);
            }
        }
        CertCloseStore(store, 0);
#else
        SSL_CTX_set_default_verify_paths(context);
#endif
    }

    // a write to a closed peer raises SIGPIPE on the writing thread; tls writes can't pass MSG_NOSIGNAL
    void blockSigpipe() {
#ifndef _WIN32
        thread_local bool blocked = false;
        if (blocked)
            return;
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGPIPE);
        pthread_sigmask(SIG_BLOCK, &set, nullptr);
        blocked = true;
#endif
    }

    int sendFlags() {
#ifdef MSG_NOSIGNAL
        return MSG_NOSIGNAL;
#else
        return 0;
#endif
    }
}

NetStream::~NetStream() {
    close();
}

void NetStream::_fail(int code, const std::string &message) {
    if (mErrorCode != 0 || !mErrorMessage.empty())
        return;
    mErrorCode = code;
    mErrorMessage = message;
}

void NetStream::_failTls(const char *what) {
    const unsigned long code = ERR_get_error();
    if (code == 0) {
        _fail(-1, what);
        return;
    }
    char buffer[256];
    ERR_error_string_n(code, buffer, sizeof(buffer));
    _fail((int) ERR_GET_REASON(code), buffer);
}

int NetStream::_remainingMilliseconds() const {
    if (!mHasDeadline)
        return -1;
    const auto now = std::chrono::steady_clock::now();
    if (now >= mDeadline)
        return 0;
    // at least 1: a wait of 0 would spin until the deadline
    const long long left = std::chrono::duration_cast<std::chrono::milliseconds>(mDeadline - now).count();
    return (int) std::min<long long>(std::max<long long>(left, 1), INT_MAX);
}

bool NetStream::_expired() {
    if (mHasDeadline && std::chrono::steady_clock::now() >= mDeadline) {
        _fail(timedOutError(), socketErrorText(timedOutError()));
        return true;
    }
    return false;
}

bool NetStream::_wait(bool write) {
    if (_expired())
        return false;
    const int ready = mAborted.load() ? 0 : waitSocket((NativeSocket) mSocket, write, _remainingMilliseconds());
    if (mAborted.load()) {
        _fail(-1, "aborted");
        return false;
    }
    if (ready > 0)
        return true;
    if (ready == 0) {
        _fail(timedOutError(), socketErrorText(timedOutError()));
        return false;
    }
    const int error = lastSocketError();
    _fail(error, socketErrorText(error));
    return false;
}

void NetStream::clearDeadline() {
    mHasDeadline = false;
}

bool NetStream::open(const std::string &host, const std::string &port, bool secure, uint32_t timeoutSeconds) {
    initialiseSockets();
    blockSigpipe();
    mHasDeadline = timeoutSeconds != 0;
    mDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);

    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *results = nullptr;
    const int resolved = getaddrinfo(host.c_str(), port.c_str(), &hints, &results);
    if (resolved != 0) {
        _fail(resolved, gai_strerror(resolved));
        return false;
    }

    NativeSocket connected = NET_INVALID_SOCKET;
    int lastError = 0;
    for (struct addrinfo *entry = results; entry != nullptr && !mAborted.load(); entry = entry->ai_next) {
        NativeSocket descriptor = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
        if (descriptor == NET_INVALID_SOCKET) {
            lastError = lastSocketError();
            continue;
        }
        {
            std::lock_guard<std::mutex> lock(mSocketMutex);
            mSocket = (intptr_t) descriptor;
        }

        setBlocking(descriptor, false);
        bool established = connect(descriptor, entry->ai_addr, (int) entry->ai_addrlen) == 0;
        if (!established) {
            const int ready = waitSocket(descriptor, true, _remainingMilliseconds());
            if (ready > 0) {
                int error = 0;
                socklen_t length = sizeof(error);
                if (getsockopt(descriptor, SOL_SOCKET, SO_ERROR, (char *) &error, &length) == 0 && error == 0)
                    established = true;
                else
                    lastError = error;
            } else {
                lastError = ready == 0 ? timedOutError() : lastSocketError();
            }
        }

        if (!established) {
            std::lock_guard<std::mutex> lock(mSocketMutex);
            NET_CLOSE_SOCKET(descriptor);
            mSocket = -1;
            continue;
        }

        // left non-blocking: every wait goes through poll and the deadline
        setNoSigpipe(descriptor);
        connected = descriptor;
        break;
    }
    freeaddrinfo(results);

    if (connected == NET_INVALID_SOCKET || mAborted.load()) {
        if (lastError == 0)
            lastError = -1;
        _fail(lastError, mAborted.load() ? std::string("aborted") : socketErrorText(lastError));
        return false;
    }

    if (!secure)
        return true;

    SSL_CTX *context = SSL_CTX_new(TLS_client_method());
    if (context == nullptr) {
        _failTls("tls context");
        return false;
    }
    mContext = context;
    SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION);
    loadSystemTrustStore(context);
    SSL_CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);

    SSL *ssl = SSL_new(context);
    if (ssl == nullptr) {
        _failTls("tls session");
        return false;
    }
    mSsl = ssl;
    SSL_set_fd(ssl, (int) connected);
    SSL_set_tlsext_host_name(ssl, host.c_str());
    SSL_set_hostflags(ssl, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    SSL_set1_host(ssl, host.c_str());
    for (;;) {
        const int result = SSL_connect(ssl);
        if (result == 1)
            return true;
        const int reason = SSL_get_error(ssl, result);
        if ((reason != SSL_ERROR_WANT_READ && reason != SSL_ERROR_WANT_WRITE)
            || !_wait(reason == SSL_ERROR_WANT_WRITE)) {
            _failTls("tls handshake");
            return false;
        }
    }
}

bool NetStream::writeAll(const char *data, size_t size) {
    size_t written = 0;
    while (written < size) {
        if (mAborted.load()) {
            _fail(-1, "aborted");
            return false;
        }
        if (_expired())
            return false;
        int sent = 0;
        if (mSsl != nullptr) {
            sent = SSL_write((SSL *) mSsl, data + written, (int) (size - written));
            if (sent <= 0) {
                const int reason = SSL_get_error((SSL *) mSsl, sent);
                if (reason == SSL_ERROR_WANT_READ || reason == SSL_ERROR_WANT_WRITE) {
                    if (!_wait(reason == SSL_ERROR_WANT_WRITE))
                        return false;
                    continue;
                }
                _failTls("tls write");
                return false;
            }
        } else {
            sent = (int) ::send((NativeSocket) mSocket, data + written, (int) (size - written), sendFlags());
            if (sent <= 0) {
                const int error = lastSocketError();
                if (sent < 0 && wouldBlock(error)) {
                    if (!_wait(true))
                        return false;
                    continue;
                }
                _fail(error, socketErrorText(error));
                return false;
            }
        }
        written += (size_t) sent;
    }
    return true;
}

int NetStream::read(char *buffer, size_t size) {
    for (;;) {
        if (mAborted.load() || mSocket == -1)
            return -1;
        // checked before every read: a peer trickling bytes must not push the deadline back
        if (_expired())
            return -1;
        if (mSsl != nullptr) {
            const int read = SSL_read((SSL *) mSsl, buffer, (int) size);
            if (read > 0)
                return read;
            const int reason = SSL_get_error((SSL *) mSsl, read);
            if (reason == SSL_ERROR_ZERO_RETURN)
                return 0;
            if (reason == SSL_ERROR_WANT_READ || reason == SSL_ERROR_WANT_WRITE) {
                if (!_wait(reason == SSL_ERROR_WANT_WRITE))
                    return -1;
                continue;
            }
            if (reason == SSL_ERROR_SYSCALL && ERR_peek_error() == 0) {
                const int error = lastSocketError();
                if (read == 0 || error == 0)
                    return 0;
                _fail(error, socketErrorText(error));
                return -1;
            }
            _failTls("tls read");
            return -1;
        }
        const int read = (int) ::recv((NativeSocket) mSocket, buffer, (int) size, 0);
        if (read >= 0)
            return read;
        const int error = lastSocketError();
        if (wouldBlock(error)) {
            if (!_wait(false))
                return -1;
            continue;
        }
        _fail(error, socketErrorText(error));
        return -1;
    }
}

bool NetStream::waitReadable(int milliseconds) {
    if (mAborted.load() || mSocket == -1)
        return false;
    if (mSsl != nullptr && SSL_pending((SSL *) mSsl) > 0)
        return true;
    return waitSocket((NativeSocket) mSocket, false, milliseconds) > 0;
}

void NetStream::abort() {
    mAborted.store(true);
    std::lock_guard<std::mutex> lock(mSocketMutex);
    if (mSocket != -1) {
#ifdef _WIN32
        ::shutdown((NativeSocket) mSocket, SD_BOTH);
#else
        ::shutdown((NativeSocket) mSocket, SHUT_RDWR);
#endif
    }
}

void NetStream::close() {
    if (mSsl != nullptr) {
        if (!mAborted.load())
            SSL_shutdown((SSL *) mSsl);
        SSL_free((SSL *) mSsl);
        mSsl = nullptr;
    }
    if (mContext != nullptr) {
        SSL_CTX_free((SSL_CTX *) mContext);
        mContext = nullptr;
    }
    std::lock_guard<std::mutex> lock(mSocketMutex);
    if (mSocket != -1) {
        NET_CLOSE_SOCKET((NativeSocket) mSocket);
        mSocket = -1;
    }
}
