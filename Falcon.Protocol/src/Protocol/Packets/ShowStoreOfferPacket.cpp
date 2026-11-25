#include "Protocol/Packets/ShowStoreOfferPacket.h"

#include "Core/Utility/UUID.h"
#include "Protocol/NetworkPacketHandler.h"

ShowStoreOfferPacket::ShowStoreOfferPacket() = default;

void ShowStoreOfferPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUuid(Uuid::fromString(mOfferId));
    stream.putByte((unsigned char) mRedirectType);
}

void ShowStoreOfferPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mOfferId = stream.getUuid().toString();
    mRedirectType = (StoreOfferRedirectType) stream.getByte();
}

void ShowStoreOfferPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
