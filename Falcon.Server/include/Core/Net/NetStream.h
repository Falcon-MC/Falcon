#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

// one tcp connection, tls when secure; reads and writes block until the deadline set by open
class NetStream {
public:
    NetStream() = default;

    ~NetStream();

    NetStream(const NetStream &) = delete;

    NetStream &operator=(const NetStream &) = delete;

    // timeoutSeconds bounds open and every read and write after it, counted from this call; 0: no deadline.
    // on failure errorCode and errorMessage come from the platform or openssl
    bool open(const std::string &host, const std::string &port, bool secure, uint32_t timeoutSeconds);

    bool writeAll(const char *data, size_t size);

    // >0 bytes read, 0 the peer closed, -1 an error or the timeout
    int read(char *buffer, size_t size);

    // true when a read would not block; waits at most milliseconds
    bool waitReadable(int milliseconds);

    // reads and writes wait without a time limit from now on
    void clearDeadline();

    // any thread: unblocks a pending read or write, the stream fails from then on
    void abort();

    void close();

    bool isAborted() const {
        return mAborted.load();
    }

    int getErrorCode() const {
        return mErrorCode;
    }

    const std::string &getErrorMessage() const {
        return mErrorMessage;
    }

private:
    void _fail(int code, const std::string &message);

    void _failTls(const char *what);

    // -1 no deadline
    int _remainingMilliseconds() const;

    // fails the stream once the deadline passed
    bool _expired();

    // until the socket is ready or the deadline; false fails the stream
    bool _wait(bool write);

    std::mutex mSocketMutex;
    intptr_t mSocket = -1;
    void *mContext = nullptr;
    void *mSsl = nullptr;
    std::atomic<bool> mAborted{false};
    std::chrono::steady_clock::time_point mDeadline;
    bool mHasDeadline = false;
    int mErrorCode = 0;
    std::string mErrorMessage;
};
