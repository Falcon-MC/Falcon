#pragma once

#include "Network/Client/ClientTransport.h"

#include <atomic>

class ServerTransport : public ClientTransport {
public:
    ServerTransport() : mConnected(true), mCloseReason((int) DisconnectFailReason::Unknown) {
    }

    void runEvents() override {
    }

    void close() override {
        if (!mConnected.exchange(false))
            return;

        mCloseReason.store((int) DisconnectFailReason::Disconnected);
        _closeTransport();
    }

    bool isConnected() const override {
        return mConnected.load();
    }

    DisconnectFailReason getCloseReason() const override {
        return (DisconnectFailReason) mCloseReason.load();
    }

    void markClosed(DisconnectFailReason reason) {
        mCloseReason.store((int) reason);
        mConnected.store(false);
    }

protected:
    virtual void _closeTransport() = 0;

private:
    std::atomic<bool> mConnected;
    std::atomic<int> mCloseReason;
};
