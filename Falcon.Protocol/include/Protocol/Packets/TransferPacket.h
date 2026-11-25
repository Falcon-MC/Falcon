#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/GatheringsConfigurationJoinInfo.h"

#include <cstdint>
#include <string>

class TransferPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::Transfer;

    TransferPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "TransferPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mAddress;
    int32_t mPort = 0;
    bool mReloadWorld = false;
    bool mHasGatheringsConfigurationJoinInfo = false;
    GatheringsConfigurationJoinInfo mGatheringsConfigurationJoinInfo;
};
