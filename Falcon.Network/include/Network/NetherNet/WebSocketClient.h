#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;

namespace nethernet {
    class WebSocketClient {
    public:
        typedef std::function<void(const std::string &message)> MessageHandler;
        typedef std::function<void(const std::string &reason)> CloseHandler;

        WebSocketClient();

        ~WebSocketClient();

        void addHeader(const std::string &name, const std::string &value);

        void setSubprotocol(const std::string &subprotocol);

        bool connect(const std::string &url, const MessageHandler &onMessage, const CloseHandler &onClose);

        bool sendText(const std::string &payload);

        void close();

        bool isOpen() const { return mOpen.load(); }

    private:
        bool _handshake(const std::string &host, const std::string &path, const std::string &key);

        bool _writeAll(const std::string &data);

        bool _readExact(size_t length, std::string &out);

        bool _writeFrame(unsigned char opcode, const std::string &payload);

        void _readLoop();

        static std::string _generateKey();

        static bool _parseUrl(const std::string &url, std::string &host, std::string &port, std::string &path,
                              bool &secure);

        std::vector<std::pair<std::string, std::string>> mHeaders;
        std::string mSubprotocol;

        long long mSocket;
        SSL_CTX *mContext;
        SSL *mSsl;

        MessageHandler mOnMessage;
        CloseHandler mOnClose;

        std::thread mThread;
        std::mutex mWriteMutex;
        std::atomic<bool> mOpen;
        std::string mPending;
        unsigned char mPendingOpcode;
    };
}
