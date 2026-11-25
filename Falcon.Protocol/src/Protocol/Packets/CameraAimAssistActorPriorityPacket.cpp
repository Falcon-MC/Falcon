#include "Protocol/Packets/CameraAimAssistActorPriorityPacket.h"

#include "Protocol/NetworkPacketHandler.h"

CameraAimAssistActorPriorityPacket::CameraAimAssistActorPriorityPacket() = default;

void CameraAimAssistActorPriorityPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mPriorityData.size());
    for (const AimAssistActorPriorityData &data: mPriorityData) {
        stream.putLInt((uint32_t) data.mPresetIndex);
        stream.putLInt((uint32_t) data.mCategoryIndex);
        stream.putLInt((uint32_t) data.mActorIndex);
        stream.putLInt((uint32_t) data.mPriorityValue);
    }
}

void CameraAimAssistActorPriorityPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    uint32_t length = stream.getArrayLength();
    mPriorityData.reserve(length);
    for (uint32_t i = 0; i < length; i++) {
        AimAssistActorPriorityData data;
        data.mPresetIndex = (int32_t) stream.getLInt();
        data.mCategoryIndex = (int32_t) stream.getLInt();
        data.mActorIndex = (int32_t) stream.getLInt();
        data.mPriorityValue = (int32_t) stream.getLInt();
        mPriorityData.push_back(data);
    }
}

void CameraAimAssistActorPriorityPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
