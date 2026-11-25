#pragma once

enum class TransportLayer : int {
    Unknown = 0,
    RakNet = 1,
    NetherNet = 2
};

enum class Compressibility : unsigned char {
    Compressible = 0,
    Incompressible = 1
};

enum class ConnectionType : int {
    Unknown = 0,
    Local = 1,
    Remote = 2
};

enum class DisconnectFailReason : int {
    Unknown = 0,
    Disconnected = 1,
    Timeout = 2,
    ServerFull = 3,
    IncompatibleProtocolVersion = 4,
    ConnectionRefused = 5,
    ShuttingDown = 6
};

const char *toString(DisconnectFailReason reason);

const char *toString(TransportLayer layer);
