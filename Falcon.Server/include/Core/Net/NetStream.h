#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

// one tcp connection, tls when secure; reads and writes block up to the timeout given to open
class NetStream {
public:
    NetStream() = default;

    ~NetStream();

    NetStream(const NetStream &) = delete;

    NetStream &operator=(const NetStream &) = delete;

    // timeoutSeconds 0: no timeout; on failure errorCode and errorMessage come from the platform or openssl
    bool open(const std::string &host, const std::string &port, bool secure, uint32_t timeoutSeconds);

    bool writeAll(const char *data, size_t size);

    // >0 bytes read, 0 the peer closed, -1 an error or the timeout
    int read(char *buffer, size_t size);

    // true when a read would not block; waits at most milliseconds
    bool waitReadable(int milliseconds);

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

    std::mutex mSocketMutex;
    intptr_t mSocket = -1;
    void *mContext = nullptr;
    void *mSsl = nullptr;
    std::atomic<bool> mAborted{false};
    int mErrorCode = 0;
    std::string mErrorMessage;
};
