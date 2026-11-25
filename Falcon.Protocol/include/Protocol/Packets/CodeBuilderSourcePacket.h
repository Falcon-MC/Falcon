#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/CodeBuilderData.h"

class CodeBuilderSourcePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::CodeBuilderSource;

    CodeBuilderSourcePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "CodeBuilderSourcePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    CodeBuilderOperationType mOperation = CodeBuilderOperationType::None;
    CodeBuilderCategoryType mCategory = CodeBuilderCategoryType::None;
    CodeBuilderCodeStatus mCodeStatus = CodeBuilderCodeStatus::None;
};
