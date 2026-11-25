#include "Network/NetworkEnums.h"

const char *toString(DisconnectFailReason reason) {
    switch (reason) {
        case DisconnectFailReason::Disconnected:
            return "Disconnected";
        case DisconnectFailReason::Timeout:
            return "Timeout";
        case DisconnectFailReason::ServerFull:
            return "ServerFull";
        case DisconnectFailReason::IncompatibleProtocolVersion:
            return "IncompatibleProtocolVersion";
        case DisconnectFailReason::ConnectionRefused:
            return "ConnectionRefused";
        case DisconnectFailReason::ShuttingDown:
            return "ShuttingDown";
        default:
            return "Unknown";
    }
}

const char *toString(TransportLayer layer) {
    switch (layer) {
        case TransportLayer::RakNet:
            return "RakNet";
        case TransportLayer::NetherNet:
            return "NetherNet";
        default:
            return "Unknown";
    }
}
