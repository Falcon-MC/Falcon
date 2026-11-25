#pragma once

#include "Network/NetherNet/NetherNetSignaling.h"
#include "Network/NetherNet/WebSocketClient.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

class MinecraftAuthentication;

namespace nethernet {
    class WebSocketSignaling : public Signaling {
    public:
        WebSocketSignaling();

        ~WebSocketSignaling() override;

        bool connect(MinecraftAuthentication &authentication, const std::string &networkID, std::string &outError);

        bool signal(const Signal &signal, unsigned int timeoutMs, std::string &outError) override;

        bool requestCredentials(Credentials &outCredentials, unsigned int timeoutMs, std::string &outError) override;

        std::string getNetworkID() const override;

        void close() override;

    private:
        void _onMessage(const std::string &message);

        void _pingLoop();

        bool _write(int type, const std::string &to, const std::string &message, const std::string &messageID);

        WebSocketClient mSocket;
        std::string mNetworkID;
        Credentials mCredentials;
        bool mHasCredentials;

        std::thread mPingThread;
        std::mutex mPingMutex;
        std::condition_variable mPingSignal;
        bool mStopping;
    };
}
