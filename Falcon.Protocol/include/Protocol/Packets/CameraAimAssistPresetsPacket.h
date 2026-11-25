#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/CameraAimAssistTypes.h"

#include <vector>

class CameraAimAssistPresetsPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::CameraAimAssistPresets;

    CameraAimAssistPresetsPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "CameraAimAssistPresetsPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    std::vector<CameraAimAssistCategory> mCategoryDefinitions;
    std::vector<CameraAimAssistPresetDefinition> mPresets;
    CameraAimAssistOperation mOperation = CameraAimAssistOperation::Set;
};
