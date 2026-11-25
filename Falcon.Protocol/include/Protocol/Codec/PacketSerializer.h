#pragma once

#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"

class Packet;

class PacketCodecContext;

/**
 * Reads and writes the body of one packet type in the format of one protocol version. Packets whose format
 * did not change keep their own read and write; a version only provides a serializer for the packets it changes.
 */
class PacketSerializer {
public:
    virtual ~PacketSerializer() = default;

    virtual void write(const Packet &packet, BinaryStream &stream, const PacketCodecContext &context) const = 0;

    virtual void read(Packet &packet, ReadOnlyBinaryStream &stream, const PacketCodecContext &context) const = 0;
};

/**
 * Uses the packet's own read and write, which follow the current protocol.
 */
class DefaultPacketSerializer : public PacketSerializer {
public:
    void write(const Packet &packet, BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(Packet &packet, ReadOnlyBinaryStream &stream, const PacketCodecContext &context) const override;
};
