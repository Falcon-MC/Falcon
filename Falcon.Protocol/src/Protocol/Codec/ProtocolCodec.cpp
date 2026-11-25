#include "Protocol/Codec/ProtocolCodec.h"

#include "Protocol/Packet.h"

#include <utility>

ProtocolCodec::ProtocolCodec(ProtocolCapabilities capabilities) : mCapabilities(std::move(capabilities)) {}

void ProtocolCodec::registerPacket(MinecraftPacketIds id, PacketFactory factory,
                                   std::shared_ptr<const PacketSerializer> serializer) {
    if (!factory)
        return;

    mEntries[(int) id] = Entry{std::move(factory), std::move(serializer)};
}

void ProtocolCodec::removePacket(MinecraftPacketIds id) {
    mEntries.erase((int) id);
}

bool ProtocolCodec::supports(MinecraftPacketIds id) const {
    return mEntries.find((int) id) != mEntries.end();
}

std::shared_ptr<Packet> ProtocolCodec::createPacket(MinecraftPacketIds id) const {
    const auto it = mEntries.find((int) id);
    if (it == mEntries.end())
        return nullptr;

    return it->second.mFactory();
}

void ProtocolCodec::write(const Packet &packet, BinaryStream &stream, const PacketCodecContext &context) const {
    packet.writeHeader(stream);
    _serializerFor(packet.getId()).write(packet, stream, context);
}

void ProtocolCodec::read(Packet &packet, ReadOnlyBinaryStream &stream, const PacketCodecContext &context) const {
    _serializerFor(packet.getId()).read(packet, stream, context);
}

const PacketSerializer &ProtocolCodec::_serializerFor(MinecraftPacketIds id) const {
    const auto it = mEntries.find((int) id);
    if (it == mEntries.end() || it->second.mSerializer == nullptr)
        return mDefaultSerializer;

    return *it->second.mSerializer;
}
