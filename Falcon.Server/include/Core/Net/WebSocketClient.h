#pragma once

#include "Core/Net/HttpsClient.h"
#include "Core/Net/NetStream.h"

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// a ws or wss client (RFC 6455) on its own thread; the owner polls it from the game thread
class WebSocketClient {
public:
    struct Event {
        enum class Kind { Opened, Failed, Message, Closed };

        Kind mKind = Kind::Message;
        std::string mText;
        // Failed: the handshake status when the server answered, 0 for a network error
        int mStatus = 0;
        int mErrorCode = 0;
        // Closed: the server closed it or the connection dropped
        bool mByServer = true;
        // Closed: a message went past setMaxMessageBytes()
        bool mTooLarge = false;
    };

    WebSocketClient() = default;

    ~WebSocketClient();

    WebSocketClient(const WebSocketClient &) = delete;

    WebSocketClient &operator=(const WebSocketClient &) = delete;

    // before start(); 0: no limit. a longer message closes the socket before it is read in
    void setMaxMessageBytes(uint64_t bytes) {
        mMaxMessageBytes = bytes;
    }

    void start(const std::string &url, const HttpsClient::Headers &headers);

    // queued for the io thread; ignored once closed
    void send(const std::string &text);

    // a close frame, then the socket goes away
    void close();

    std::vector<Event> poll();

private:
    void _run(std::string url, HttpsClient::Headers headers);

    bool _handshake(const std::string &url, const HttpsClient::Headers &headers, Event &failure);

    bool _writeFrame(uint8_t opcode, const std::string &payload);

    // false once the connection is gone
    bool _readFrames(std::string &buffer);

    void _push(Event event);

    NetStream mStream;
    std::thread mThread;
    std::mutex mMutex;
    std::deque<std::string> mOutgoing;
    std::vector<Event> mEvents;
    bool mCloseRequested = false;
    bool mFinished = false;
    // fragments of a message that is not complete yet
    std::string mPartial;
    std::string mMessage;
    uint64_t mMaxMessageBytes = 0;
    bool mTooLarge = false;
};
