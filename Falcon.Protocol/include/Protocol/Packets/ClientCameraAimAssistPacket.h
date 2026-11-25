#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/CameraAimAssistTypes.h"

#include <string>

class ClientCameraAimAssistPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::ClientCameraAimAssist;

    ClientCameraAimAssistPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "ClientCameraAimAssistPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::string mPresetId;
    AimAssistAction mAction = AimAssistAction::Set;
    bool mAllowAimAssist = false;
};
