#pragma once

#include "Protocol/Packet.h"

class AddBehaviorTreePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::AddBehaviorTree;

    AddBehaviorTreePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "AddBehaviorTreePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mBehaviorTreeJson;
};
