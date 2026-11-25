#include "Protocol/Packets/CameraPresetsPacket.h"

#include "Protocol/CameraCodec.h"
#include "Protocol/NetworkPacketHandler.h"

CameraPresetsPacket::CameraPresetsPacket() = default;

void CameraPresetsPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mPresets.size());
    for (const CameraPreset &preset: mPresets) {
        CameraCodec::writePreset(stream, context, preset);
    }
}

void CameraPresetsPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    uint32_t length = stream.getArrayLength();
    mPresets.reserve(length);
    for (uint32_t i = 0; i < length; i++) {
        mPresets.push_back(CameraCodec::readPreset(stream, context));
    }
}

void CameraPresetsPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
