#include "Protocol/Packets/LevelEventGenericPacket.h"

#include "Core/NBT/NbtIo.h"
#include "Protocol/NetworkPacketHandler.h"

LevelEventGenericPacket::LevelEventGenericPacket()
        : mEventId(0), mData(Tag::ofCompound()) {
}

void LevelEventGenericPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    (void) context;

    stream.putVarInt(mEventId);
    NbtIo::writeValue(stream, mData, NbtVariant::Network);
}

void LevelEventGenericPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    (void) context;

    mEventId = stream.getVarInt();
    mData = NbtIo::readValue(stream, Tag::Type::Compound, NbtVariant::Network);
}

void LevelEventGenericPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
