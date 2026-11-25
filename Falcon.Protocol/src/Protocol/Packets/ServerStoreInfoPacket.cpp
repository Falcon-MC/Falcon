#include "Protocol/Packets/ServerStoreInfoPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ServerStoreInfoPacket::ServerStoreInfoPacket() = default;

void ServerStoreInfoPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putOptionalPresent(mHasStore);
    if (!mHasStore)
        return;

    stream.putString(mStoreId);
    stream.putString(mStoreName);
}

void ServerStoreInfoPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mHasStore = stream.getOptionalPresent();
    if (!mHasStore)
        return;

    mStoreId = stream.getString();
    mStoreName = stream.getString();
}

void ServerStoreInfoPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
