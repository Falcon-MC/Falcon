#pragma once

#include "Network/NetworkEnums.h"

class ClientTransport {
public:
    virtual ~ClientTransport() = default;

    virtual void runEvents() = 0;

    virtual void close() = 0;

    virtual bool isConnected() const = 0;

    virtual DisconnectFailReason getCloseReason() const = 0;
};
