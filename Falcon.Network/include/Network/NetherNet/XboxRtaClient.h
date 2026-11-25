#pragma once

#include "Network/NetherNet/WebSocketClient.h"

#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace nethernet {
    extern const char *RTA_CONNECT_URL;
    extern const char *RTA_SUBPROTOCOL;

    enum RtaStatus : int {
        RtaStatusOK = 0,
        RtaStatusUnknownResource = 1,
        RtaStatusSubscriptionLimitReached = 2,
        RtaStatusNoResourceData = 3,
        RtaStatusThrottled = 1001,
        RtaStatusServiceUnavailable = 1002
    };

    struct RtaSubscription {
        unsigned int mId = 0;
        std::string mCustom;
    };

    class XboxRtaClient {
    public:
        typedef std::function<void(unsigned int subscriptionID, const std::string &custom)> EventHandler;

        XboxRtaClient();

        ~XboxRtaClient();

        bool connect(const std::string &authorization, const EventHandler &onEvent);

        bool subscribe(const std::string &resourceUri, RtaSubscription &out, int timeoutSeconds = 15);

        bool unsubscribe(unsigned int subscriptionID, int timeoutSeconds = 15);

        void close();

        bool isOpen() const { return mSocket.isOpen(); }

    private:
        struct Pending {
            bool mReceived = false;
            int mStatus = 0;
            std::vector<std::string> mPayload;
        };

        void _onMessage(const std::string &message);

        static std::vector<std::string> _splitTopLevel(const std::string &array);

        static std::string _unquote(const std::string &value);

        WebSocketClient mSocket;
        EventHandler mOnEvent;

        std::mutex mMutex;
        std::condition_variable mSignal;
        std::map<unsigned long long, std::shared_ptr<Pending>> mPending;
        unsigned int mSequences[2];
    };
}
