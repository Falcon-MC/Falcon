#include "Protocol/Packet.h"

#include "Protocol/Codec/ProtocolCodec.h"
#include "Protocol/NetworkPacketHandler.h"

Packet::Packet()
        : mCompressible(Compressibility::Compressible), mReliability(Reliability::ReliableOrdered),
          mClientSubId(0), mSenderSubId(0), mIsHandled(false) {}

void Packet::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    (void) id;
    (void) handler;
}

void Packet::writeHeader(BinaryStream &stream) const {
    const uint32_t header = (uint32_t) getId() | ((uint32_t) mSenderSubId << 10) | ((uint32_t) mClientSubId << 12);
    stream.putUnsignedVarInt(header);
}

void Packet::writeWithHeader(BinaryStream &stream, const PacketCodecContext &context) const {
    context.getCodec().write(*this, stream, context);
}

void Packet::readHeader(ReadOnlyBinaryStream &stream) {
    peekId(stream, mSenderSubId, mClientSubId);
}

MinecraftPacketIds Packet::peekId(ReadOnlyBinaryStream &stream, unsigned char &outSenderSubId,
                                  unsigned char &outClientSubId) {
    const uint32_t header = stream.getUnsignedVarInt();

    outSenderSubId = (unsigned char) ((header >> 10) & 3);
    outClientSubId = (unsigned char) ((header >> 12) & 3);
    return (MinecraftPacketIds) (header & 0x3ff);
}
