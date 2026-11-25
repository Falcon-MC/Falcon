#pragma once

#include "Protocol/Packet.h"

class NetworkSettingsPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::NetworkSettings;

    enum class CompressionAlgorithm : int {
        ZLib,
        Snappy,
        None
    };

    NetworkSettingsPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "NetworkSettingsPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    uint16_t mCompressionThreshold;
    CompressionAlgorithm mCompressionAlgorithm;
    bool mClientThrottleEnabled;
    int32_t mClientThrottleThreshold;
    float mClientThrottleScalar;
};
