#pragma once

#include "Protocol/Packet.h"
#include "Protocol/Types/CameraTypes.h"

class CameraInstructionPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::CameraInstruction;

    CameraInstructionPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "CameraInstructionPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    bool mHasSetInstruction = false;
    CameraSetInstruction mSetInstruction;

    bool mHasClear = false;
    bool mClear = false;

    bool mHasFadeInstruction = false;
    CameraFadeInstruction mFadeInstruction;

    bool mHasTargetInstruction = false;
    CameraTargetInstruction mTargetInstruction;

    bool mHasRemoveTarget = false;
    bool mRemoveTarget = false;

    bool mHasFovInstruction = false;
    CameraFovInstruction mFovInstruction;

    bool mHasSplineInstruction = false;
    CameraSplineInstruction mSplineInstruction;

    bool mHasAttachInstruction = false;
    CameraAttachToEntityInstruction mAttachInstruction;

    bool mHasDetachFromActor = false;
    bool mDetachFromActor = false;
};
