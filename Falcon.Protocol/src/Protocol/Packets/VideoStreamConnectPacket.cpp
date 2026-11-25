#include "Protocol/Packets/VideoStreamConnectPacket.h"

#include "Protocol/NetworkPacketHandler.h"

VideoStreamConnectPacket::VideoStreamConnectPacket() = default;

void VideoStreamConnectPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mAddress);
    stream.putLFloat(mScreenshotFrequency);
    stream.putByte((unsigned char) mAction);
    stream.putLInt((uint32_t) mWidth);
    stream.putLInt((uint32_t) mHeight);
}

void VideoStreamConnectPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mAddress = stream.getString();
    mScreenshotFrequency = stream.getLFloat();
    mAction = (VideoStreamConnectAction) stream.getByte();
    mWidth = (int32_t) stream.getLInt();
    mHeight = (int32_t) stream.getLInt();
}

void VideoStreamConnectPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
