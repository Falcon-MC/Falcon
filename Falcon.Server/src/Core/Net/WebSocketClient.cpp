#include "Core/Net/WebSocketClient.h"

#include "Core/Net/NetUri.h"

#include <algorithm>
#include <cctype>
#include <random>

#include <openssl/evp.h>
#include <openssl/sha.h>

namespace {
    const char *const ACCEPT_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const size_t MAX_HEAD_SIZE = 64 * 1024;
    const int POLL_MILLISECONDS = 20;

    std::string lower(std::string text) {
        for (char &c: text)
            c = (char) std::tolower((unsigned char) c);
        return text;
    }

    std::string base64(const unsigned char *data, size_t size) {
        std::string out(4 * ((size + 2) / 3) + 1, '\0');
        const int written = EVP_EncodeBlock((unsigned char *) &out[0], data, (int) size);
        out.resize((size_t) written);
        return out;
    }

    const std::string *findHeader(const HttpsClient::Headers &headers, const std::string &name) {
        for (const auto &header: headers) {
            if (lower(header.first) == name)
                return &header.second;
        }
        return nullptr;
    }
}

WebSocketClient::~WebSocketClient() {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mCloseRequested = true;
    }
    mStream.abort();
    if (mThread.joinable())
        mThread.join();
}

void WebSocketClient::start(const std::string &url, const HttpsClient::Headers &headers) {
    mThread = std::thread(&WebSocketClient::_run, this, url, headers);
}

void WebSocketClient::send(const std::string &text) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mCloseRequested && !mFinished)
        mOutgoing.push_back(text);
}

void WebSocketClient::close() {
    std::lock_guard<std::mutex> lock(mMutex);
    mCloseRequested = true;
}

std::vector<WebSocketClient::Event> WebSocketClient::poll() {
    std::lock_guard<std::mutex> lock(mMutex);
    std::vector<Event> events;
    events.swap(mEvents);
    return events;
}

void WebSocketClient::_push(Event event) {
    std::lock_guard<std::mutex> lock(mMutex);
    mEvents.push_back(std::move(event));
}

bool WebSocketClient::_handshake(const std::string &url, const HttpsClient::Headers &headers, Event &failure) {
    failure.mKind = Event::Kind::Failed;
    const NetUri uri = NetUri::split(url);
    const std::string scheme = lower(uri.mScheme);
    const bool secure = scheme == "wss";
    std::string host;
    std::string port;
    uri.hostAndPort(host, port);
    if (port.empty())
        port = secure ? "443" : "80";

    if (!mStream.open(host, port, secure, 0)) {
        failure.mErrorCode = mStream.getErrorCode();
        failure.mText = mStream.getErrorMessage();
        return false;
    }

    std::random_device device;
    unsigned char nonce[16];
    for (unsigned char &byte: nonce)
        byte = (unsigned char) device();
    const std::string key = base64(nonce, sizeof(nonce));

    std::string request = "GET " + uri.target() + " HTTP/1.1\r\n";
    request += "Host: " + uri.mAuthority + "\r\n";
    request += "Upgrade: websocket\r\nConnection: Upgrade\r\n";
    request += "Sec-WebSocket-Key: " + key + "\r\nSec-WebSocket-Version: 13\r\n";
    for (const auto &header: headers)
        request += header.first + ": " + header.second + "\r\n";
    request += "\r\n";

    std::string head;
    std::string rest;
    HttpsClient::Headers answer;
    int status = 0;
    if (!mStream.writeAll(request.data(), request.size()) || !HttpsClient::readHead(mStream, head, rest, MAX_HEAD_SIZE)
        || !HttpsClient::parseHead(head, status, answer)) {
        failure.mErrorCode = mStream.getErrorCode() != 0 ? mStream.getErrorCode() : -1;
        failure.mText = !mStream.getErrorMessage().empty() ? mStream.getErrorMessage()
                                                           : std::string("malformed handshake response");
        return false;
    }
    if (status != 101) {
        failure.mStatus = status;
        return false;
    }

    std::string expected = key + ACCEPT_GUID;
    unsigned char digest[SHA_DIGEST_LENGTH];
    SHA1((const unsigned char *) expected.data(), expected.size(), digest);
    const std::string *accept = findHeader(answer, "sec-websocket-accept");
    const std::string *upgrade = findHeader(answer, "upgrade");
    if (accept == nullptr || *accept != base64(digest, sizeof(digest)) || upgrade == nullptr
        || lower(*upgrade) != "websocket") {
        failure.mErrorCode = -1;
        failure.mText = "invalid handshake response";
        return false;
    }
    mPartial = rest;
    return true;
}

bool WebSocketClient::_writeFrame(uint8_t opcode, const std::string &payload) {
    std::string frame;
    frame.push_back((char) (0x80 | opcode));
    const uint64_t size = payload.size();
    // a client masks every frame it sends
    if (size < 126) {
        frame.push_back((char) (0x80 | size));
    } else if (size <= 0xFFFF) {
        frame.push_back((char) (0x80 | 126));
        frame.push_back((char) (size >> 8));
        frame.push_back((char) size);
    } else {
        frame.push_back((char) (0x80 | 127));
        for (int shift = 56; shift >= 0; shift -= 8)
            frame.push_back((char) (size >> shift));
    }
    std::random_device device;
    unsigned char mask[4];
    for (unsigned char &byte: mask)
        byte = (unsigned char) device();
    frame.append((const char *) mask, 4);
    for (size_t index = 0; index < payload.size(); ++index)
        frame.push_back((char) (payload[index] ^ mask[index % 4]));
    return mStream.writeAll(frame.data(), frame.size());
}

bool WebSocketClient::_readFrames(std::string &buffer) {
    for (;;) {
        if (buffer.size() < 2)
            return true;
        const uint8_t first = (uint8_t) buffer[0];
        const uint8_t second = (uint8_t) buffer[1];
        uint64_t size = second & 0x7F;
        size_t offset = 2;
        if (size == 126) {
            if (buffer.size() < 4)
                return true;
            size = ((uint64_t) (uint8_t) buffer[2] << 8) | (uint8_t) buffer[3];
            offset = 4;
        } else if (size == 127) {
            if (buffer.size() < 10)
                return true;
            size = 0;
            for (int index = 0; index < 8; ++index)
                size = (size << 8) | (uint8_t) buffer[2 + index];
            offset = 10;
        }
        const bool masked = (second & 0x80) != 0;
        unsigned char mask[4] = {0, 0, 0, 0};
        if (masked) {
            if (buffer.size() < offset + 4)
                return true;
            for (int index = 0; index < 4; ++index)
                mask[index] = (unsigned char) buffer[offset + index];
            offset += 4;
        }
        const uint8_t frameOpcode = first & 0x0F;
        const uint64_t held = std::min<uint64_t>(mMaxMessageBytes, mMessage.size());
        if (mMaxMessageBytes != 0 && frameOpcode <= 0x2 && size > mMaxMessageBytes - held) {
            // 1009: message too big
            _writeFrame(0x8, std::string("\x03\xF1", 2));
            mTooLarge = true;
            return false;
        }
        if (size > buffer.size() - offset)
            return true;

        std::string payload = buffer.substr(offset, (size_t) size);
        if (masked) {
            for (size_t index = 0; index < payload.size(); ++index)
                payload[index] = (char) (payload[index] ^ mask[index % 4]);
        }
        buffer.erase(0, offset + (size_t) size);

        const uint8_t opcode = first & 0x0F;
        const bool final = (first & 0x80) != 0;
        switch (opcode) {
            case 0x0:
            case 0x1:
            case 0x2:
                mMessage += payload;
                if (final) {
                    Event event;
                    event.mKind = Event::Kind::Message;
                    event.mText.swap(mMessage);
                    _push(std::move(event));
                }
                break;
            case 0x8:
                _writeFrame(0x8, payload.substr(0, 2));
                return false;
            case 0x9:
                if (!_writeFrame(0xA, payload))
                    return false;
                break;
            default:
                break;
        }
    }
}

void WebSocketClient::_run(std::string url, HttpsClient::Headers headers) {
    Event failure;
    if (!_handshake(url, headers, failure)) {
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mFinished = true;
        }
        _push(failure);
        mStream.close();
        return;
    }
    Event opened;
    opened.mKind = Event::Kind::Opened;
    _push(opened);

    std::string buffer;
    buffer.swap(mPartial);
    bool byServer = true;
    bool alive = _readFrames(buffer);
    while (alive) {
        std::deque<std::string> outgoing;
        bool closing = false;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            outgoing.swap(mOutgoing);
            closing = mCloseRequested;
        }
        for (const std::string &text: outgoing) {
            if (!_writeFrame(0x1, text)) {
                alive = false;
                break;
            }
        }
        if (!alive)
            break;
        if (closing) {
            // 1000: normal closure
            _writeFrame(0x8, std::string("\x03\xE8", 2));
            byServer = false;
            break;
        }
        if (!mStream.waitReadable(POLL_MILLISECONDS))
            continue;
        char chunk[16384];
        const int read = mStream.read(chunk, sizeof(chunk));
        if (read <= 0)
            break;
        buffer.append(chunk, (size_t) read);
        alive = _readFrames(buffer);
    }

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mFinished = true;
        if (mCloseRequested)
            byServer = false;
    }
    Event closed;
    closed.mKind = Event::Kind::Closed;
    closed.mByServer = byServer && !mTooLarge;
    closed.mTooLarge = mTooLarge;
    _push(closed);
    mStream.close();
}
