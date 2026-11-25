#pragma once

#include "Protocol/Packet.h"

#include <cstdint>

class ServerboundLoadingScreenPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ServerboundLoadingScreen;

    enum class Type {
        Unknown,
        StartLoadingScreen,
        EndLoadingScreen,
    };

    ServerboundLoadingScreenPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ServerboundLoadingScreenPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Type mType = Type::Unknown;

    bool mHasLoadingScreenId = false;
    int32_t mLoadingScreenId = 0;
};
