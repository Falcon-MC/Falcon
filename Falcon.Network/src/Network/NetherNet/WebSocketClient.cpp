#include "Network/NetherNet/WebSocketClient.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#endif

#include "Core/Debug/BedrockLog.h"
#include "Network/Http/TlsRoots.h"
#include "Network/NetherNet/NetherNetIdentity.h"

#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include <cctype>
#include <cstring>

#ifdef _WIN32
typedef SOCKET FalconWebSocket;
#define FALCON_WEBSOCKET_INVALID INVALID_SOCKET
#define FALCON_WEBSOCKET_CLOSE ::closesocket
#else
typedef int FalconWebSocket;
#define FALCON_WEBSOCKET_INVALID (-1)
#define FALCON_WEBSOCKET_CLOSE ::close
#endif

namespace nethernet {

    namespace {

        const unsigned char OPCODE_CONTINUATION = 0x0;
        const unsigned char OPCODE_TEXT = 0x1;
        const unsigned char OPCODE_BINARY = 0x2;
        const unsigned char OPCODE_CLOSE = 0x8;
        const unsigned char OPCODE_PING = 0x9;
        const unsigned char OPCODE_PONG = 0xa;

        const char *WEBSOCKET_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        const size_t MAX_FRAME_PAYLOAD = 8 * 1024 * 1024;

        std::string toLower(const std::string &value) {
            std::string out;
            out.reserve(value.size());

            for (char c: value)
                out.push_back((char) tolower((unsigned char) c));

            return out;
        }

    }

    WebSocketClient::WebSocketClient()
            : mSocket(-1), mContext(nullptr), mSsl(nullptr), mOpen(false), mPendingOpcode(0) {
    }

    WebSocketClient::~WebSocketClient() {
        close();
    }

    void WebSocketClient::addHeader(const std::string &name, const std::string &value) {
        mHeaders.push_back(std::make_pair(name, value));
    }

    void WebSocketClient::setSubprotocol(const std::string &subprotocol) {
        mSubprotocol = subprotocol;
    }

    bool WebSocketClient::_parseUrl(const std::string &url, std::string &host, std::string &port, std::string &path,
                                    bool &secure) {
        std::string rest;

        if (url.rfind("wss://", 0) == 0) {
            secure = true;
            rest = url.substr(6);
        } else if (url.rfind("ws://", 0) == 0) {
            secure = false;
            rest = url.substr(5);
        } else {
            return false;
        }

        const size_t slash = rest.find('/');
        std::string authority = slash == std::string::npos ? rest : rest.substr(0, slash);
        path = slash == std::string::npos ? "/" : rest.substr(slash);

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

    std::string WebSocketClient::_generateKey() {
        unsigned char raw[16];

        if (RAND_bytes(raw, sizeof(raw)) != 1)
            memset(raw, 0, sizeof(raw));

        return Identity::encodeBase64(std::string((const char *) raw, sizeof(raw)));
    }

    bool WebSocketClient::_writeAll(const std::string &data) {
        size_t sent = 0;

        while (sent < data.size()) {
            const int written = SSL_write(mSsl, data.data() + sent, (int) (data.size() - sent));
            if (written <= 0)
                return false;

            sent += (size_t) written;
        }

        return true;
    }

    bool WebSocketClient::_readExact(size_t length, std::string &out) {
        out.clear();
        out.reserve(length);

        char buffer[4096];

        while (out.size() < length) {
            const size_t wanted = length - out.size();
            const int read = SSL_read(mSsl, buffer, (int) (wanted < sizeof(buffer) ? wanted : sizeof(buffer)));
            if (read <= 0)
                return false;

            out.append(buffer, (size_t) read);
        }

        return true;
    }

    bool WebSocketClient::_handshake(const std::string &host, const std::string &path, const std::string &key) {
        std::string request = "GET " + path + " HTTP/1.1\r\n";
        request += "Host: " + host + "\r\n";
        request += "Upgrade: websocket\r\n";
        request += "Connection: Upgrade\r\n";
        request += "Sec-WebSocket-Key: " + key + "\r\n";
        request += "Sec-WebSocket-Version: 13\r\n";

        if (!mSubprotocol.empty())
            request += "Sec-WebSocket-Protocol: " + mSubprotocol + "\r\n";

        for (const std::pair<std::string, std::string> &header: mHeaders)
            request += header.first + ": " + header.second + "\r\n";

        request += "\r\n";

        if (!_writeAll(request))
            return false;

        std::string response;
        char buffer[1024];

        while (response.find("\r\n\r\n") == std::string::npos) {
            const int read = SSL_read(mSsl, buffer, sizeof(buffer));
            if (read <= 0)
                return false;

            response.append(buffer, (size_t) read);

            if (response.size() > 32 * 1024)
                return false;
        }

        const size_t lineEnd = response.find("\r\n");
        const std::string statusLine = response.substr(0, lineEnd);

        if (statusLine.find(" 101") == std::string::npos) {
            LOG_WARN(LogAreaID::Network, "WebSocket upgrade refused: %s", statusLine.c_str());
            return false;
        }

        const std::string expected = key + WEBSOCKET_GUID;
        unsigned char digest[SHA_DIGEST_LENGTH];
        SHA1((const unsigned char *) expected.data(), expected.size(), digest);

        const std::string accept = Identity::encodeBase64(std::string((const char *) digest, sizeof(digest)));
        const std::string lowered = toLower(response.substr(0, response.find("\r\n\r\n")));
        const size_t position = lowered.find("sec-websocket-accept:");

        if (position == std::string::npos) {
            LOG_WARN(LogAreaID::Network, "WebSocket upgrade response has no Sec-WebSocket-Accept header");
            return false;
        }

        const size_t end = lowered.find("\r\n", position);
        std::string value = response.substr(position + 21, end - position - 21);

        while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
            value.erase(value.begin());
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r'))
            value.pop_back();

        if (value != accept) {
            LOG_WARN(LogAreaID::Network, "WebSocket upgrade response has an invalid Sec-WebSocket-Accept header");
            return false;
        }

        mPending = response.substr(response.find("\r\n\r\n") + 4);
        return true;
    }

    bool WebSocketClient::connect(const std::string &url, const MessageHandler &onMessage,
                                  const CloseHandler &onClose) {
        if (mOpen.load())
            return false;

        std::string host;
        std::string port;
        std::string path;
        bool secure = false;

        if (!_parseUrl(url, host, port, path, secure)) {
            LOG_ERROR(LogAreaID::Network, "Invalid WebSocket URL: %s", url.c_str());
            return false;
        }

        if (!secure) {
            LOG_ERROR(LogAreaID::Network, "Only wss:// WebSocket URLs are supported: %s", url.c_str());
            return false;
        }

        addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        addrinfo *result = nullptr;
        if (getaddrinfo(host.c_str(), port.c_str(), &hints, &result) != 0 || result == nullptr) {
            LOG_ERROR(LogAreaID::Network, "Could not resolve %s", host.c_str());
            return false;
        }

        FalconWebSocket descriptor = FALCON_WEBSOCKET_INVALID;

        for (addrinfo *entry = result; entry != nullptr; entry = entry->ai_next) {
            descriptor = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
            if (descriptor == FALCON_WEBSOCKET_INVALID)
                continue;

            if (::connect(descriptor, entry->ai_addr, (int) entry->ai_addrlen) == 0)
                break;

            FALCON_WEBSOCKET_CLOSE(descriptor);
            descriptor = FALCON_WEBSOCKET_INVALID;
        }

        freeaddrinfo(result);

        if (descriptor == FALCON_WEBSOCKET_INVALID) {
            LOG_ERROR(LogAreaID::Network, "Could not connect to %s:%s", host.c_str(), port.c_str());
            return false;
        }

        mContext = SSL_CTX_new(TLS_client_method());
        if (mContext == nullptr) {
            FALCON_WEBSOCKET_CLOSE(descriptor);
            return false;
        }

        SSL_CTX_set_min_proto_version(mContext, TLS1_2_VERSION);
        SSL_CTX_set_verify(mContext, SSL_VERIFY_PEER, nullptr);
        TlsRoots::load(mContext);

        mSsl = SSL_new(mContext);
        if (mSsl == nullptr) {
            SSL_CTX_free(mContext);
            mContext = nullptr;
            FALCON_WEBSOCKET_CLOSE(descriptor);
            return false;
        }

        SSL_set_fd(mSsl, (int) descriptor);
        SSL_set_tlsext_host_name(mSsl, host.c_str());
        SSL_set_hostflags(mSsl, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
        SSL_set1_host(mSsl, host.c_str());

        mSocket = (long long) descriptor;

        if (SSL_connect(mSsl) != 1) {
            LOG_ERROR(LogAreaID::Network, "TLS handshake with %s failed", host.c_str());
            close();
            return false;
        }

        if (!_handshake(host, path, _generateKey())) {
            close();
            return false;
        }

        mOnMessage = onMessage;
        mOnClose = onClose;
        mOpen.store(true);
        mThread = std::thread(&WebSocketClient::_readLoop, this);
        return true;
    }

    bool WebSocketClient::_writeFrame(unsigned char opcode, const std::string &payload) {
        std::string frame;
        frame.push_back((char) (unsigned char) (0x80 | opcode));

        unsigned char mask[4];
        if (RAND_bytes(mask, sizeof(mask)) != 1)
            memset(mask, 0, sizeof(mask));

        const size_t length = payload.size();

        if (length < 126) {
            frame.push_back((char) (unsigned char) (0x80 | length));
        } else if (length <= 0xffff) {
            frame.push_back((char) (unsigned char) (0x80 | 126));
            frame.push_back((char) (unsigned char) ((length >> 8) & 0xff));
            frame.push_back((char) (unsigned char) (length & 0xff));
        } else {
            frame.push_back((char) (unsigned char) (0x80 | 127));
            for (int i = 7; i >= 0; i--)
                frame.push_back((char) (unsigned char) ((length >> (i * 8)) & 0xff));
        }

        frame.append((const char *) mask, sizeof(mask));

        for (size_t i = 0; i < length; i++)
            frame.push_back((char) (unsigned char) ((unsigned char) payload[i] ^ mask[i % 4]));

        std::lock_guard<std::mutex> lock(mWriteMutex);
        return _writeAll(frame);
    }

    bool WebSocketClient::sendText(const std::string &payload) {
        if (!mOpen.load())
            return false;

        return _writeFrame(OPCODE_TEXT, payload);
    }

    void WebSocketClient::_readLoop() {
        std::string assembled;
        unsigned char assembledOpcode = 0;

        while (mOpen.load()) {
            std::string header;

            if (!_readExact(2, header))
                break;

            const unsigned char first = (unsigned char) header[0];
            const unsigned char second = (unsigned char) header[1];
            const bool fin = (first & 0x80) != 0;
            const unsigned char opcode = first & 0x0f;
            const bool masked = (second & 0x80) != 0;

            size_t length = second & 0x7f;

            if (length == 126) {
                std::string extended;
                if (!_readExact(2, extended))
                    break;

                length = ((size_t) (unsigned char) extended[0] << 8) | (unsigned char) extended[1];
            } else if (length == 127) {
                std::string extended;
                if (!_readExact(8, extended))
                    break;

                length = 0;
                for (int i = 0; i < 8; i++)
                    length = (length << 8) | (unsigned char) extended[i];
            }

            if (length > MAX_FRAME_PAYLOAD)
                break;

            std::string mask;
            if (masked && !_readExact(4, mask))
                break;

            std::string payload;
            if (length > 0 && !_readExact(length, payload))
                break;

            if (masked) {
                for (size_t i = 0; i < payload.size(); i++)
                    payload[i] = (char) ((unsigned char) payload[i] ^ (unsigned char) mask[i % 4]);
            }

            if (opcode == OPCODE_CLOSE) {
                _writeFrame(OPCODE_CLOSE, std::string());
                break;
            }

            if (opcode == OPCODE_PING) {
                _writeFrame(OPCODE_PONG, payload);
                continue;
            }

            if (opcode == OPCODE_PONG)
                continue;

            if (opcode == OPCODE_CONTINUATION) {
                assembled += payload;
            } else {
                assembled = payload;
                assembledOpcode = opcode;
            }

            if (!fin)
                continue;

            if (assembledOpcode == OPCODE_TEXT && mOnMessage != nullptr)
                mOnMessage(assembled);
            else if (assembledOpcode == OPCODE_BINARY)
                LOG_TRACE(LogAreaID::Network, "Ignoring a binary WebSocket frame");

            assembled.clear();
        }

        const bool wasOpen = mOpen.exchange(false);

        if (wasOpen && mOnClose != nullptr)
            mOnClose("websocket closed");
    }

    void WebSocketClient::close() {
        const bool wasOpen = mOpen.exchange(false);

        if (wasOpen)
            _writeFrame(OPCODE_CLOSE, std::string());

        if (mSocket >= 0) {
            FALCON_WEBSOCKET_CLOSE((FalconWebSocket) mSocket);
            mSocket = -1;
        }

        if (mThread.joinable() && mThread.get_id() != std::this_thread::get_id())
            mThread.join();

        if (mSsl != nullptr) {
            SSL_free(mSsl);
            mSsl = nullptr;
        }

        if (mContext != nullptr) {
            SSL_CTX_free(mContext);
            mContext = nullptr;
        }
    }
}
