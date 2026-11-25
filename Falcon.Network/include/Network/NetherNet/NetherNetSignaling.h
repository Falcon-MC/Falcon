#pragma once

#include "Network/NetherNet/NetherNetCredentials.h"
#include "Network/NetherNet/NetherNetSignal.h"

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>

namespace nethernet {
    class Signaling {
    public:
        typedef std::function<void(const Signal &signal)> SignalHandler;

        Signaling();

        virtual ~Signaling();

        Signaling(const Signaling &) = delete;

        Signaling &operator=(const Signaling &) = delete;

        virtual bool signal(const Signal &signal, unsigned int timeoutMs, std::string &outError) = 0;

        virtual bool requestCredentials(Credentials &outCredentials, unsigned int timeoutMs,
                                        std::string &outError) = 0;

        virtual std::string getNetworkID() const = 0;

        virtual bool isTrickleIceDisabled() const;

        virtual void close() = 0;

        unsigned int subscribe(const SignalHandler &handler);

        void unsubscribe(unsigned int id);

        bool isClosed() const;

        std::string getCloseReason() const;

        static uint64_t generateNetworkID();

    protected:
        void _dispatch(const Signal &signal);

        void _markClosed(const std::string &reason);

        void _expectDelivery(const std::string &messageID);

        void _completeDelivery(const std::string &messageID, const std::string &error);

        bool _awaitDelivery(const std::string &messageID, unsigned int timeoutMs, std::string &outError);

        mutable std::mutex mStateMutex;
        std::condition_variable mStateSignal;
        bool mClosed;

    private:
        struct Delivery {
            bool mDone = false;
            std::string mError;
        };

        std::mutex mHandlerMutex;
        std::map<unsigned int, SignalHandler> mHandlers;
        unsigned int mNextHandlerID;
        std::map<std::string, Delivery> mDeliveries;
        std::string mCloseReason;
    };
}
