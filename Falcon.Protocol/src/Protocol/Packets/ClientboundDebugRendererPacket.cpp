#include "Protocol/Packets/ClientboundDebugRendererPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ClientboundDebugRendererPacket::ClientboundDebugRendererPacket() = default;

void ClientboundDebugRendererPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putLInt((uint32_t) mDebugMarkerType);

    if (mDebugMarkerType == ClientboundDebugRendererType::AddDebugMarkerCube) {
        stream.putString(mMarkerText);
        stream.putVector3f(mMarkerPosition);
        stream.putFloat(mMarkerColorRed);
        stream.putFloat(mMarkerColorGreen);
        stream.putFloat(mMarkerColorBlue);
        stream.putFloat(mMarkerColorAlpha);
        stream.putLLong((uint64_t) mMarkerDuration);
    }
}

void ClientboundDebugRendererPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mDebugMarkerType = (ClientboundDebugRendererType) stream.getLInt();

    if (mDebugMarkerType == ClientboundDebugRendererType::AddDebugMarkerCube) {
        mMarkerText = stream.getString();
        mMarkerPosition = stream.getVector3f();
        mMarkerColorRed = stream.getFloat();
        mMarkerColorGreen = stream.getFloat();
        mMarkerColorBlue = stream.getFloat();
        mMarkerColorAlpha = stream.getFloat();
        mMarkerDuration = (int64_t) stream.getLLong();
    }
}

void ClientboundDebugRendererPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
