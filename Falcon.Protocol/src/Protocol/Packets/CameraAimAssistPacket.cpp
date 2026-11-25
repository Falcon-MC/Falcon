#include "Protocol/Packets/CameraAimAssistPacket.h"

#include "Protocol/NetworkPacketHandler.h"

CameraAimAssistPacket::CameraAimAssistPacket() = default;

void CameraAimAssistPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mPresetId);
    stream.putVector2f(mViewAngle);
    stream.putLFloat(mDistance);
    stream.putByte((unsigned char) mTargetMode);
    stream.putByte((unsigned char) mAction);
    stream.putBool(mShowDebugRender);
}

void CameraAimAssistPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mPresetId = stream.getString();
    mViewAngle = stream.getVector2f();
    mDistance = stream.getLFloat();
    mTargetMode = (TargetMode) stream.getByte();
    mAction = (AimAssistAction) stream.getByte();
    mShowDebugRender = stream.getBool();
}

void CameraAimAssistPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
