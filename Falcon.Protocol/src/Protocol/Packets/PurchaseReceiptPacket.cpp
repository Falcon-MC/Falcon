#include "Protocol/Packets/PurchaseReceiptPacket.h"

#include "Protocol/NetworkPacketHandler.h"

PurchaseReceiptPacket::PurchaseReceiptPacket() = default;

void PurchaseReceiptPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putArrayLength((uint32_t) mReceipts.size());
    for (const std::string &receipt: mReceipts) {
        stream.putString(receipt);
    }
}

void PurchaseReceiptPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    uint32_t count = stream.getArrayLength();
    mReceipts.clear();
    mReceipts.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
        mReceipts.push_back(stream.getString());
    }
}

void PurchaseReceiptPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
