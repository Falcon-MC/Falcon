#include "Protocol/Packets/ServerboundLoadingScreenPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ServerboundLoadingScreenPacket::ServerboundLoadingScreenPacket() = default;

void ServerboundLoadingScreenPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt((int32_t) mType);
    stream.putOptionalPresent(mHasLoadingScreenId);
    if (mHasLoadingScreenId) {
        stream.putLInt((uint32_t) mLoadingScreenId);
    }
}

void ServerboundLoadingScreenPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mType = (Type) stream.getVarInt();
    mHasLoadingScreenId = stream.getOptionalPresent();
    if (mHasLoadingScreenId) {
        mLoadingScreenId = (int32_t) stream.getLInt();
    }
}

void ServerboundLoadingScreenPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
