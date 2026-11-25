#pragma once

#include "Network/NetherNet/NetherNetSignaling.h"
#include "Network/NetherNet/WebSocketClient.h"

#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>

class MinecraftAuthentication;

namespace nethernet {
    class JsonRpcSignaling : public Signaling {
    public:
        JsonRpcSignaling();

        ~JsonRpcSignaling() override;

        bool connect(MinecraftAuthentication &authentication, const std::string &networkID, std::string &outError);

        bool signal(const Signal &signal, unsigned int timeoutMs, std::string &outError) override;

        bool requestCredentials(Credentials &outCredentials, unsigned int timeoutMs, std::string &outError) override;

        std::string getNetworkID() const override;

        void close() override;

        const std::string &getPlayerMessagingID() const;

        static bool isMessagingID(const std::string &value);

        static std::string normalizeMessagingID(const std::string &value);

    private:
        struct PendingCall {
            bool mDone = false;
            std::string mResult;
            std::string mError;
        };

        bool _call(const std::string &method, const std::string &params, unsigned int timeoutMs,
                   std::string *outResult, std::string &outError);

        bool _post(const std::string &method, const std::string &params);

        bool _sendEnvelope(const std::string &messageID, const std::string &inner, const std::string &recipient,
                           bool awaitResponse, unsigned int timeoutMs, std::string &outError);

        void _onMessage(const std::string &message);

        void _handleEnvelope(const std::string &envelope);

        void _pingLoop();

        WebSocketClient mSocket;
        std::string mNetworkID;
        std::string mPlayerMessagingID;

        std::map<unsigned long long, PendingCall> mCalls;
        unsigned long long mNextCallID;

        std::mutex mCredentialsMutex;
        Credentials mCredentials;
        int64_t mCredentialsExpiry;

        std::thread mPingThread;
        std::mutex mPingMutex;
        std::condition_variable mPingSignal;
        bool mStopping;
        int mPingFrequencySeconds;
    };
}
