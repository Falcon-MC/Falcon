#include "Protocol/Packets/MapInfoRequestPacket.h"

#include "Core/Utility/BinaryDataException.h"

#include <string>

#include "Protocol/NetworkPacketHandler.h"

MapInfoRequestPacket::MapInfoRequestPacket() = default;

void MapInfoRequestPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarLong(mUniqueMapId);

    stream.putLInt((uint32_t) mPixels.size());
    for (const MapPixel &pixel : mPixels) {
        stream.putLInt((uint32_t) pixel.mPixel);
        stream.putLShort((uint16_t) pixel.mIndex);
    }
}

void MapInfoRequestPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mUniqueMapId = stream.getVarLong();

    uint32_t count = stream.getLInt();
    // six bytes a pixel: a count the packet cannot hold must not size the allocation
    if (count > stream.getRemainingLength() / 6)
        throw BinaryDataException("Map pixel count " + std::to_string(count) + " exceeds the packet");
    mPixels.clear();
    mPixels.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        MapPixel pixel;
        pixel.mPixel = (int32_t) stream.getLInt();
        pixel.mIndex = stream.getLShort();
        mPixels.push_back(pixel);
    }
}

void MapInfoRequestPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
