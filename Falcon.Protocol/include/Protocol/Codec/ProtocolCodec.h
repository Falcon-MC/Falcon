#pragma once

#include "Protocol/Codec/PacketSerializer.h"
#include "Protocol/Codec/ProtocolCapabilities.h"
#include "Protocol/MinecraftPacketIds.h"

#include <functional>
#include <memory>
#include <unordered_map>

class Packet;

class PacketCodecContext;

/**
 * Everything one protocol version needs to exchange packets: which packets exist, how to create them from their
 * network ID and how to serialize each of them. Handlers keep working with the same packet classes whatever the
 * version; only the serializers differ. A codec is filled once and then shared read-only between connections.
 */
class ProtocolCodec {
public:
    typedef std::function<std::shared_ptr<Packet>()> PacketFactory;

    explicit ProtocolCodec(ProtocolCapabilities capabilities);

    void registerPacket(MinecraftPacketIds id, PacketFactory factory,
                        std::shared_ptr<const PacketSerializer> serializer = nullptr);

    template<class PacketType>
    void registerPacket(std::shared_ptr<const PacketSerializer> serializer = nullptr) {
        registerPacket(PacketType::ID, []() {
            return std::static_pointer_cast<Packet>(std::make_shared<PacketType>());
        }, std::move(serializer));
    }

    /**
     * Drops a packet this version does not have, so nothing of that type is sent to its clients.
     */
    void removePacket(MinecraftPacketIds id);

    const ProtocolCapabilities &getCapabilities() const { return mCapabilities; }

    int getProtocolVersion() const { return mCapabilities.mProtocolVersion; }

    bool supports(MinecraftPacketIds id) const;

    std::shared_ptr<Packet> createPacket(MinecraftPacketIds id) const;

    /**
     * Writes the packet header followed by its body in this version's format.
     */
    void write(const Packet &packet, BinaryStream &stream, const PacketCodecContext &context) const;

    /**
     * Reads the packet body in this version's format; the header must already have been read.
     */
    void read(Packet &packet, ReadOnlyBinaryStream &stream, const PacketCodecContext &context) const;

private:
    struct Entry {
        PacketFactory mFactory;
        std::shared_ptr<const PacketSerializer> mSerializer;
    };

    const PacketSerializer &_serializerFor(MinecraftPacketIds id) const;

    ProtocolCapabilities mCapabilities;
    std::unordered_map<int, Entry> mEntries;
    DefaultPacketSerializer mDefaultSerializer;
};
