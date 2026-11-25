#include "Protocol/Packets/PlayerVideoCapturePacket.h"

#include "Protocol/NetworkPacketHandler.h"

PlayerVideoCapturePacket::PlayerVideoCapturePacket() = default;

void PlayerVideoCapturePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) mAction);

    if (mAction == PlayerVideoCaptureAction::StartVideoCapture) {
        stream.putLInt((uint32_t) mFrameRate);
        stream.putString(mFilePrefix);
    }
}

void PlayerVideoCapturePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    unsigned char action = stream.getByte();
    mAction = action > (unsigned char) PlayerVideoCaptureAction::StartVideoCapture
              ? PlayerVideoCaptureAction::Unknown
              : (PlayerVideoCaptureAction) action;

    if (mAction == PlayerVideoCaptureAction::StartVideoCapture) {
        mFrameRate = (int32_t) stream.getLInt();
        mFilePrefix = stream.getString();
    }
}

void PlayerVideoCapturePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
