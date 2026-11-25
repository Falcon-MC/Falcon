#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/CameraAimAssistTypes.h"

#include <vector>

class CameraAimAssistActorPriorityPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::CameraAimAssistActorPriority;

    CameraAimAssistActorPriorityPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "CameraAimAssistActorPriorityPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<AimAssistActorPriorityData> mPriorityData;
};
