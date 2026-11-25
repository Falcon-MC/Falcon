#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/AgentActionType.h"

#include <string>

class AgentActionEventPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::AgentActionEvent;

    AgentActionEventPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "AgentActionEventPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mRequestId;
    AgentActionType mActionType = AgentActionType::None;
    std::string mResponseJson;
};
