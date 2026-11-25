#include "Network/Http/HttpClient.h"
#include "Network/Http/TlsRoots.h"

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wincrypt.h>

#undef X509_NAME
#undef X509_EXTENSIONS
#undef PKCS7_SIGNER_INFO
#undef OCSP_REQUEST
#undef OCSP_RESPONSE

#else

#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#endif

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <algorithm>
#include <chrono>
#include <limits>

#ifdef _WIN32
typedef SOCKET HttpSocket;
#define HTTP_SOCKET_INVALID INVALID_SOCKET
#define HTTP_SOCKET_CLOSE ::closesocket
#else
typedef int HttpSocket;
#define HTTP_SOCKET_INVALID (-1)
#define HTTP_SOCKET_CLOSE ::close
#endif

namespace {



    std::string toLower(const std::string &value) {
        std::string result;
        result.reserve(value.size());

        for (char character: value)
            result.push_back((char) tolower((unsigned char) character));

        return result;
    }

    std::string trim(const std::string &value) {
        size_t start = 0;
        size_t end = value.size();

        while (start < end && (value[start] == ' ' || value[start] == '\t'))
            ++start;

        while (end > start && (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r'))
            --end;

        return value.substr(start, end - start);
    }

    void initializeSockets() {
#ifdef _WIN32
        static std::once_flag flag;
        std::call_once(flag, []() {
            WSADATA data;
            WSAStartup(MAKEWORD(2, 2), &data);
        });
#endif
    }

    bool setBlocking(HttpSocket descriptor, bool blocking) {
#ifdef _WIN32
        u_long mode = blocking ? 0 : 1;
        return ioctlsocket(descriptor, FIONBIO, &mode) == 0;
#else
        const int flags = fcntl(descriptor, F_GETFL, 0);
        if (flags < 0)
            return false;

        return fcntl(descriptor, F_SETFL, blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK)) == 0;
#endif
    }

    void setTimeouts(HttpSocket descriptor, int timeoutMs) {
#ifdef _WIN32
        const DWORD value = (DWORD) timeoutMs;
        setsockopt(descriptor, SOL_SOCKET, SO_RCVTIMEO, (const char *) &value, sizeof(value));
        setsockopt(descriptor, SOL_SOCKET, SO_SNDTIMEO, (const char *) &value, sizeof(value));
#else
        timeval value;
        value.tv_sec = timeoutMs / 1000;
        value.tv_usec = (timeoutMs % 1000) * 1000;
        setsockopt(descriptor, SOL_SOCKET, SO_RCVTIMEO, &value, sizeof(value));
        setsockopt(descriptor, SOL_SOCKET, SO_SNDTIMEO, &value, sizeof(value));
#endif
    }

    bool connectWithTimeout(HttpSocket descriptor, const sockaddr *address, int addressLength, int timeoutMs) {
        if (!setBlocking(descriptor, false))
            return false;

        const int result = ::connect(descriptor, address, addressLength);

        if (result != 0) {
#ifdef _WIN32
            if (WSAGetLastError() != WSAEWOULDBLOCK)
                return false;
#else
            if (errno != EINPROGRESS)
                return false;
#endif

            fd_set writeSet;
            FD_ZERO(&writeSet);
            FD_SET(descriptor, &writeSet);

            fd_set errorSet;
            FD_ZERO(&errorSet);
            FD_SET(descriptor, &errorSet);

            timeval timeout;
            timeout.tv_sec = timeoutMs / 1000;
            timeout.tv_usec = (timeoutMs % 1000) * 1000;

            if (select((int) descriptor + 1, nullptr, &writeSet, &errorSet, &timeout) <= 0)
                return false;

            if (FD_ISSET(descriptor, &errorSet))
                return false;

            int socketError = 0;
#ifdef _WIN32
            int length = sizeof(socketError);
            getsockopt(descriptor, SOL_SOCKET, SO_ERROR, (char *) &socketError, &length);
#else
            socklen_t length = sizeof(socketError);
            getsockopt(descriptor, SOL_SOCKET, SO_ERROR, &socketError, &length);
#endif
            if (socketError != 0)
                return false;
        }

        return setBlocking(descriptor, true);
    }

    bool isResponseComplete(const std::string &raw, size_t &chunkPosition) {
        const size_t headerEnd = raw.find("\r\n\r\n");
        if (headerEnd == std::string::npos)
            return false;

        const std::string headers = toLower(raw.substr(0, headerEnd));
        const size_t bodyLength = raw.size() - headerEnd - 4;

        if (headers.find("transfer-encoding: chunked") != std::string::npos) {
            size_t position = chunkPosition ? chunkPosition : headerEnd + 4;
            for (;;) {
                size_t lineEnd = raw.find("\r\n", position);
                if (lineEnd == std::string::npos) return false;
                std::string line = raw.substr(position, lineEnd - position);
                char *end = nullptr;
                errno = 0;
                auto size = strtoull(line.c_str(), &end, 16);
                if (errno || end == line.c_str() || (*end && *end != ';')) return false;
                position = lineEnd + 2;
                if (size == 0) return raw.compare(position, 2, "\r\n") == 0 || raw.find("\r\n\r\n", position) != std::string::npos;
                if (size > raw.size() - position || raw.size() - position - size < 2) return false;
                if (raw.compare(position + static_cast<size_t>(size), 2, "\r\n") != 0) return false;
                position += static_cast<size_t>(size) + 2;
                chunkPosition = position;
            }
        }

        const size_t lengthPosition = headers.find("content-length:");
        if (lengthPosition == std::string::npos)
            return false;

        const size_t contentLength = (size_t) strtoull(headers.c_str() + lengthPosition + 15, nullptr, 10);
        return bodyLength >= contentLength;
    }

}

std::string HttpResponse::getHeader(const std::string &name) const {
    const std::string lowered = toLower(name);

    for (const std::pair<std::string, std::string> &header: mHeaders) {
        if (header.first == lowered)
            return header.second;
    }

    return std::string();
}

bool HttpClient::_parseUrl(const std::string &url, std::string &host, std::string &port, std::string &path) {
    bool secure = url.rfind("https://", 0) == 0;
    if (!secure && url.rfind("http://", 0) != 0) return false;
    const std::string rest = url.substr(secure ? 8 : 7);
    const size_t slash = rest.find_first_of("/?#");
    const std::string authority = slash == std::string::npos ? rest : rest.substr(0, slash);
    path = slash == std::string::npos ? "/" : rest.substr(slash);

    if (path.front() != '/') path.insert(path.begin(), '/');
    path = path.substr(0, path.find('#'));
    if (url.find_first_of("\r\n") != std::string::npos || authority.find('@') != std::string::npos) return false;
    const size_t colon = authority.rfind(':');
    if (colon != std::string::npos && authority.find(']') == std::string::npos) {
        host = authority.substr(0, colon);
        port = authority.substr(colon + 1);
    } else {
        host = authority;
        port = secure ? "443" : "80";
    }

    return !host.empty();
}

bool HttpClient::_decodeChunked(const std::string &body, std::string &outBody) {
    outBody.clear();
    size_t position = 0;

    for (;;) {
        const size_t lineEnd = body.find("\r\n", position);
        if (lineEnd == std::string::npos)
            return false;

        const std::string sizeText = body.substr(position, lineEnd - position);
        char *end = nullptr;
        errno = 0;
        const auto parsed = strtoull(sizeText.c_str(), &end, 16);
        if (errno || end == sizeText.c_str() || (*end && *end != ';') || sizeText.front() == '-' || parsed > std::numeric_limits<size_t>::max()) return false;
        const size_t chunkSize = static_cast<size_t>(parsed);
        position = lineEnd + 2;
        if (chunkSize == 0)
            return body.compare(position, 2, "\r\n") == 0 || body.find("\r\n\r\n", position) != std::string::npos;
        if (chunkSize > body.size() - position || body.size() - position - chunkSize < 2 || body.compare(position + chunkSize, 2, "\r\n") != 0)
            return false;

        outBody.append(body, position, chunkSize);
        position += chunkSize + 2;
    }
}

bool HttpClient::_parseResponse(const std::string &raw, HttpResponse &outResponse, std::string &outError) {
    const size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        outError = "incomplete HTTP response";
        return false;
    }

    const size_t statusEnd = raw.find("\r\n");
    const std::string statusLine = raw.substr(0, statusEnd);
    const size_t space = statusLine.find(' ');

    if (statusLine.rfind("HTTP/", 0) != 0 || space == std::string::npos) {
        outError = "invalid HTTP status line";
        return false;
    }

    outResponse.mStatus = atoi(statusLine.c_str() + space + 1);
    outResponse.mHeaders.clear();

    size_t position = statusEnd + 2;
    while (position < headerEnd) {
        size_t lineEnd = raw.find("\r\n", position);
        if (lineEnd == std::string::npos || lineEnd > headerEnd)
            lineEnd = headerEnd;

        const std::string line = raw.substr(position, lineEnd - position);
        const size_t colon = line.find(':');

        if (colon != std::string::npos)
            outResponse.mHeaders.emplace_back(toLower(trim(line.substr(0, colon))), trim(line.substr(colon + 1)));

        position = lineEnd + 2;
    }

    const std::string body = raw.substr(headerEnd + 4);

    if (toLower(outResponse.getHeader("transfer-encoding")).find("chunked") != std::string::npos) {
        if (!_decodeChunked(body, outResponse.mBody)) {
            outError = "invalid chunked HTTP body";
            return false;
        }
        return true;
    }

    const std::string contentLength = outResponse.getHeader("content-length");
    if (!contentLength.empty()) {
        char *end = nullptr;
        errno = 0;
        const auto length = strtoull(contentLength.c_str(), &end, 10);
        if (errno || end == contentLength.c_str() || *end || contentLength.front() == '-' || length > body.size()) {
            outError = "invalid or truncated HTTP content-length";
            return false;
        }
        outResponse.mBody = body.substr(0, static_cast<size_t>(length));
        return true;
    }

    outResponse.mBody = body;
    return true;
}

std::string HttpClient::encodeForm(const Headers &fields) {
    static const char HEX[] = "0123456789ABCDEF";

    std::string result;

    for (const std::pair<std::string, std::string> &field: fields) {
        if (!result.empty())
            result.push_back('&');

        for (int part = 0; part < 2; ++part) {
            const std::string &value = part == 0 ? field.first : field.second;

            for (char character: value) {
                const unsigned char byte = (unsigned char) character;

                if (isalnum(byte) || byte == '-' || byte == '_' || byte == '.' || byte == '~') {
                    result.push_back(character);
                } else if (byte == ' ') {
                    result.push_back('+');
                } else {
                    result.push_back('%');
                    result.push_back(HEX[byte >> 4]);
                    result.push_back(HEX[byte & 0x0f]);
                }
            }

            if (part == 0)
                result.push_back('=');
        }
    }

    return result;
}

bool HttpClient::request(const std::string &method, const std::string &url, const Headers &headers,
                         const std::string &body, HttpResponse &outResponse, std::string &outError,
                         int timeoutMs, size_t maxResponseSize, const std::atomic<bool> *cancel, const std::function<void(size_t)> &progress,
                         const std::string &extraRootsFile) {
    outResponse = {};
    outError.clear();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    std::string host;
    std::string port;
    std::string path;

    if (!_parseUrl(url, host, port, path)) {
        outError = "unsupported URL " + url;
        return false;
    }

    initializeSockets();

    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo *resolved = nullptr;
    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &resolved) != 0 || resolved == nullptr) {
        outError = "could not resolve " + host;
        return false;
    }

    HttpSocket descriptor = HTTP_SOCKET_INVALID;

    for (addrinfo *entry = resolved; entry != nullptr; entry = entry->ai_next) {
        descriptor = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
        if (descriptor == HTTP_SOCKET_INVALID)
            continue;

        if (cancel && cancel->load()) {
            HTTP_SOCKET_CLOSE(descriptor);
            descriptor = HTTP_SOCKET_INVALID;
            break;
        }
        int remaining = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count());
        if (remaining > 0 && connectWithTimeout(descriptor, entry->ai_addr, (int) entry->ai_addrlen, cancel ? std::min(remaining, 5000) : remaining))
            break;

        HTTP_SOCKET_CLOSE(descriptor);
        descriptor = HTTP_SOCKET_INVALID;
    }

    freeaddrinfo(resolved);

    if (descriptor == HTTP_SOCKET_INVALID) {
        outError = "could not connect to " + host + ":" + port;
        return false;
    }

    setTimeouts(descriptor, cancel ? std::min(timeoutMs, 5000) : timeoutMs);

    const bool secure = url.rfind("https://", 0) == 0;
    SSL_CTX *context = nullptr;
    SSL *ssl = nullptr;
    if (secure) {
        context = SSL_CTX_new(TLS_client_method());
        if (context == nullptr) {
            HTTP_SOCKET_CLOSE(descriptor);
            outError = "could not create TLS context";
            return false;
        }

        SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION);
        SSL_CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);
        TlsRoots::load(context);
        // a private CA beside the system roots: a test ACME server
        if (!extraRootsFile.empty() && SSL_CTX_load_verify_locations(context, extraRootsFile.c_str(), nullptr) != 1) {
            ERR_clear_error();
            SSL_CTX_free(context);
            HTTP_SOCKET_CLOSE(descriptor);
            outError = "could not read the trusted roots " + extraRootsFile;
            return false;
        }

        ssl = SSL_new(context);
        if (ssl == nullptr) {
            SSL_CTX_free(context);
            HTTP_SOCKET_CLOSE(descriptor);
            outError = "could not create TLS session";
            return false;
        }

        SSL_set_fd(ssl, (int) descriptor);
        SSL_set_tlsext_host_name(ssl, host.c_str());
        SSL_set_hostflags(ssl, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
        SSL_set1_host(ssl, host.c_str());

    }

    bool success = false;

    if (secure && SSL_connect(ssl) != 1) {
        outError = "TLS handshake with " + host + " failed";
    } else {
        std::string requestText = method + " " + path + " HTTP/1.1\r\n";
        requestText += "Host: " + host + (port == (secure ? "443" : "80") ? "" : ":" + port) + "\r\n";
        requestText += "Connection: close\r\n";

        for (const std::pair<std::string, std::string> &header: headers)
            requestText += header.first + ": " + header.second + "\r\n";

        if (!body.empty() || method == "POST" || method == "PUT")
            requestText += "Content-Length: " + std::to_string(body.size()) + "\r\n";

        requestText += "\r\n";
        requestText += body;

        size_t sent = 0;
        bool writeFailed = false;

        while (sent < requestText.size()) {
            const int written = secure ? SSL_write(ssl, requestText.data() + sent, (int) (requestText.size() - sent))
                    : static_cast<int>(::send(descriptor, requestText.data() + sent, (int) (requestText.size() - sent), 0));
            if (written <= 0) {
                writeFailed = true;
                break;
            }

            sent += (size_t) written;
        }

        if (writeFailed) {
            outError = "could not send request to " + host;
        } else {
            std::string raw;
            size_t chunkPosition = 0;
            char buffer[16384];
            bool readFailed = false;

            for (;;) {
                if ((cancel && cancel->load()) || std::chrono::steady_clock::now() >= deadline) {
                    readFailed = true;
                    outError = "HTTP request cancelled or timed out";
                    break;
                }
                const int read = secure ? SSL_read(ssl, buffer, (int) sizeof(buffer))
                        : static_cast<int>(::recv(descriptor, buffer, (int) sizeof(buffer), 0));

                if (read > 0) {
                    raw.append(buffer, (size_t) read);
                    if (progress) {
                        size_t headerEnd = raw.find("\r\n\r\n");
                        if (headerEnd != std::string::npos) progress(raw.size() - headerEnd - 4);
                    }

                    if (raw.size() > maxResponseSize) {
                        readFailed = true;
                        outError = "HTTP response from " + host + " is too large";
                        break;
                    }

                    if (isResponseComplete(raw, chunkPosition))
                        break;

                    continue;
                }

                const int error = secure ? SSL_get_error(ssl, read) : 0;
                if ((!secure && read == 0) || (secure && (error == SSL_ERROR_ZERO_RETURN || (error == SSL_ERROR_SYSCALL && !raw.empty()))))
                    break;

                readFailed = true;
                outError = "could not read response from " + host;
                break;
            }

            if (!readFailed)
                success = _parseResponse(raw, outResponse, outError);
        }

        if (ssl) SSL_shutdown(ssl);
    }

    SSL_free(ssl);
    SSL_CTX_free(context);
    HTTP_SOCKET_CLOSE(descriptor);
    return success;
}

bool HttpClient::get(const std::string &url, const Headers &headers, HttpResponse &outResponse,
                     std::string &outError, int timeoutMs, size_t maxResponseSize, const std::atomic<bool> *cancel, const std::function<void(size_t)> &progress) {
    return request("GET", url, headers, std::string(), outResponse, outError, timeoutMs, maxResponseSize, cancel, progress);
}

bool HttpClient::post(const std::string &url, const Headers &headers, const std::string &body,
                      HttpResponse &outResponse, std::string &outError, int timeoutMs) {
    return request("POST", url, headers, body, outResponse, outError, timeoutMs);
}
