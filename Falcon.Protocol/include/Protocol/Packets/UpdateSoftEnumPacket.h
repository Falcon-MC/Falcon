#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/CommandEnumData.h"

enum class SoftEnumUpdateType : unsigned char {
    Add = 0,
    Remove = 1,
    Replace = 2
};

class UpdateSoftEnumPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::UpdateSoftEnum;

    UpdateSoftEnumPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "UpdateSoftEnumPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    CommandEnumData mSoftEnum;
    SoftEnumUpdateType mType = SoftEnumUpdateType::Add;
};
