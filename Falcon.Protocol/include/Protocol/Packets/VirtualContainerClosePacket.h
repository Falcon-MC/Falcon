#pragma once

#include "Protocol/Packet.h"

#include <cstdint>

enum class VirtualContainerCloseKind : uint8_t {
    Request = 0,
    Closed = 1,
    Confirmed = 2
};

enum class VirtualContainerCloseCause : uint8_t {
    Unknown = 0,
    UserClosed = 1,
    ServerRequested = 2,
    UserBusy = 3,
    ClientNotReady = 4,
    InvalidContents = 5,
    UiCreationFailed = 6,
    UiCancelled = 7
};

class VirtualContainerClosePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::VirtualContainerClose;

    VirtualContainerClosePacket();

    MinecraftPacketIds getId() const override {
        return ID;
    }

    const char *getName() const override {
        return "VirtualContainerClosePacket";
    }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint64_t mSessionId = 1;
    VirtualContainerCloseKind mKind = VirtualContainerCloseKind::Request;
    VirtualContainerCloseCause mCause = VirtualContainerCloseCause::ServerRequested;
};
