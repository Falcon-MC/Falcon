#pragma once

#include "Core/Net/HttpsClient.h"
#include "Core/Net/NetStream.h"
#include "Core/Net/WebSocketClient.h"

#include <atomic>
#include <deque>
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// one pack's http requests and web sockets: they run on their own threads, results come back through poll
class ScriptNetService {
public:
    struct Completed {
        uint32_t mId = 0;
        HttpsClient::Response mResponse;
    };

    ScriptNetService() = default;

    ~ScriptNetService();

    ScriptNetService(const ScriptNetService &) = delete;

    ScriptNetService &operator=(const ScriptNetService &) = delete;

    void startRequest(uint32_t id, HttpsClient::Request request);

    // the exchanges finished since the last call
    std::vector<Completed> pollRequests();

    WebSocketClient &openSocket(uint32_t id, const std::string &url, const HttpsClient::Headers &headers);

    WebSocketClient *findSocket(uint32_t id);

    void removeSocket(uint32_t id);

    const std::unordered_map<uint32_t, std::unique_ptr<WebSocketClient>> &sockets() const {
        return mSockets;
    }

    // ids the pack still waits on; a request left out was cancelled and its answer is dropped
    std::unordered_set<uint32_t> mPending;
    // per socket: its uri and the bytes received during the current tick
    std::unordered_map<uint32_t, std::string> mSocketUris;
    std::unordered_map<uint32_t, uint64_t> mReceived;
    // events polled but held back: the script only subscribes once the connect promise resolved
    std::unordered_map<uint32_t, std::deque<WebSocketClient::Event>> mBacklog;

private:
    struct Job {
        uint32_t mId = 0;
        NetStream mStream;
        std::thread mThread;
        std::atomic<bool> mDone{false};
        HttpsClient::Response mResponse;
    };

    std::list<std::unique_ptr<Job>> mJobs;
    std::unordered_map<uint32_t, std::unique_ptr<WebSocketClient>> mSockets;
};
