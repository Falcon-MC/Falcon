#pragma once

#include "Core/Math/Vector3f.h"
#include "Protocol/Packet.h"
#include "Protocol/Types/CameraAimAssistTypes.h"

#include <string>

class CameraAimAssistPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::CameraAimAssist;

    enum class TargetMode {
        Angle,
        Distance,
    };

    CameraAimAssistPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "CameraAimAssistPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Vector2f mViewAngle;
    float mDistance = 0.0f;
    TargetMode mTargetMode = TargetMode::Angle;
    AimAssistAction mAction = AimAssistAction::Set;
    std::string mPresetId;
    bool mShowDebugRender = false;
};
