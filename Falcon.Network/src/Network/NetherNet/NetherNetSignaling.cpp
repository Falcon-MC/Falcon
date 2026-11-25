#include "Network/NetherNet/NetherNetSignaling.h"

#include <openssl/rand.h>

#include <chrono>
#include <random>

namespace nethernet {

    Signaling::Signaling() : mClosed(false), mNextHandlerID(0) {
    }

    Signaling::~Signaling() {
    }

    bool Signaling::isTrickleIceDisabled() const {
        return false;
    }

    uint64_t Signaling::generateNetworkID() {
        unsigned char raw[8];

        if (RAND_bytes(raw, sizeof(raw)) != 1) {
            std::random_device device;
            std::mt19937_64 engine(device());
            return engine();
        }

        uint64_t value = 0;

        for (size_t index = 0; index < sizeof(raw); index++)
            value |= ((uint64_t) raw[index]) << (index * 8);

        return value;
    }

    unsigned int Signaling::subscribe(const SignalHandler &handler) {
        std::lock_guard<std::mutex> lock(mHandlerMutex);

        const unsigned int id = mNextHandlerID++;
        mHandlers[id] = handler;
        return id;
    }

    void Signaling::unsubscribe(unsigned int id) {
        std::lock_guard<std::mutex> lock(mHandlerMutex);
        mHandlers.erase(id);
    }

    bool Signaling::isClosed() const {
        std::lock_guard<std::mutex> lock(mStateMutex);
        return mClosed;
    }

    std::string Signaling::getCloseReason() const {
        std::lock_guard<std::mutex> lock(mStateMutex);
        return mCloseReason;
    }

    void Signaling::_dispatch(const Signal &signal) {
        std::lock_guard<std::mutex> lock(mHandlerMutex);

        for (const auto &entry: mHandlers)
            entry.second(signal);
    }

    void Signaling::_markClosed(const std::string &reason) {
        {
            std::lock_guard<std::mutex> lock(mStateMutex);

            if (mClosed)
                return;

            mClosed = true;
            mCloseReason = reason;
        }

        mStateSignal.notify_all();
    }

    void Signaling::_expectDelivery(const std::string &messageID) {
        std::lock_guard<std::mutex> lock(mStateMutex);
        mDeliveries[messageID] = Delivery();
    }

    void Signaling::_completeDelivery(const std::string &messageID, const std::string &error) {
        {
            std::lock_guard<std::mutex> lock(mStateMutex);

            const auto it = mDeliveries.find(messageID);
            if (it == mDeliveries.end())
                return;

            it->second.mDone = true;
            it->second.mError = error;
        }

        mStateSignal.notify_all();
    }

    bool Signaling::_awaitDelivery(const std::string &messageID, unsigned int timeoutMs, std::string &outError) {
        std::unique_lock<std::mutex> lock(mStateMutex);

        const auto it = mDeliveries.find(messageID);
        if (it == mDeliveries.end()) {
            outError = "no delivery is expected for message " + messageID;
            return false;
        }

        const bool finished = mStateSignal.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this, &it]() {
            return it->second.mDone || mClosed;
        });

        const Delivery delivery = it->second;
        mDeliveries.erase(it);

        if (delivery.mDone) {
            if (!delivery.mError.empty()) {
                outError = delivery.mError;
                return false;
            }

            return true;
        }

        if (mClosed) {
            outError = "signaling closed: " + mCloseReason;
            return false;
        }

        outError = finished ? "delivery was not confirmed" : "timed out waiting for the delivery of message " +
                                                             messageID;
        return false;
    }
}
