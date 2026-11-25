#include "Protocol/Packets/CameraSplinePacket.h"

#include "Protocol/CameraCodec.h"
#include "Protocol/NetworkPacketHandler.h"

CameraSplinePacket::CameraSplinePacket() = default;

void CameraSplinePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mSplines.size());
    for (const CameraSplineDefinition &spline: mSplines) {
        stream.putString(spline.mName);
        CameraCodec::writeSplineInstruction(stream, spline.mInstruction);
    }
}

void CameraSplinePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    uint32_t length = stream.getArrayLength();
    mSplines.reserve(length);
    for (uint32_t i = 0; i < length; i++) {
        CameraSplineDefinition spline;
        spline.mName = stream.getString();
        spline.mInstruction = CameraCodec::readSplineInstruction(stream);
        mSplines.push_back(std::move(spline));
    }
}

void CameraSplinePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
