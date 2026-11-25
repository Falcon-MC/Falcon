#pragma once

#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Protocol/MinecraftPacketIds.h"
#include "Protocol/PacketCodecContext.h"

class NetworkIdentifier;

class NetworkPacketHandler;

class Packet {
public:
    enum class Compressibility : unsigned char {
        Compressible = 0,
        Incompressible = 1
    };

    enum class Reliability : int {
        Reliable = 0,
        ReliableOrdered = 1,
        Unreliable = 2,
        UnreliableSequenced = 3
    };

    Packet();

    virtual ~Packet() = default;

    virtual MinecraftPacketIds getId() const = 0;

    virtual const char *getName() const = 0;

    virtual void write(BinaryStream &stream, const PacketCodecContext &context) const = 0;

    virtual void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) = 0;

    virtual void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const;

    void writeHeader(BinaryStream &stream) const;

    void writeWithHeader(BinaryStream &stream, const PacketCodecContext &context) const;

    void readHeader(ReadOnlyBinaryStream &stream);

    static MinecraftPacketIds peekId(ReadOnlyBinaryStream &stream, unsigned char &outSenderSubId,
                                     unsigned char &outClientSubId);

    Compressibility mCompressible;
    Reliability mReliability;
    unsigned char mClientSubId;
    unsigned char mSenderSubId;
    bool mIsHandled;
};
