#pragma once

#include "Core/NBT/Tag.h"
#include "Protocol/Packet.h"

#include <string>

class StructureTemplateDataResponsePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::StructureTemplateDataResponse;

    enum class Type : int {
        None = 0,
        Export = 1,
        Query = 2,
        Import = 3
    };

    StructureTemplateDataResponsePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "StructureTemplateDataResponsePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mName;
    bool mSave;
    Tag mTag;
    Type mType;
};
