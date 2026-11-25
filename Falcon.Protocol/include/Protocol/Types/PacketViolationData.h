#pragma once

enum class PacketViolationType {
    Unknown,
    MalformedPacket,
};

enum class PacketViolationSeverity {
    Unknown,
    Warning,
    FinalWarning,
    TerminatingConnection,
};
