#include "Protocol/Codec/PacketSerializer.h"

#include "Protocol/Packet.h"

void DefaultPacketSerializer::write(const Packet &packet, BinaryStream &stream,
                                    const PacketCodecContext &context) const {
    packet.write(stream, context);
}

void DefaultPacketSerializer::read(Packet &packet, ReadOnlyBinaryStream &stream,
                                   const PacketCodecContext &context) const {
    packet.read(stream, context);
}
