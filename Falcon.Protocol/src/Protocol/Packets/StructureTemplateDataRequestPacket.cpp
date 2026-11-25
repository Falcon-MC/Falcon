#include "Protocol/Packets/StructureTemplateDataRequestPacket.h"

#include "Protocol/NetworkPacketHandler.h"

StructureTemplateDataRequestPacket::StructureTemplateDataRequestPacket()
        : mOperation(Operation::None) {}

void StructureTemplateDataRequestPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putString(mName);
    stream.putBlockPosition(mPosition);
    mSettings.write(stream);
    stream.putByte((unsigned char) mOperation);
}

void StructureTemplateDataRequestPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mName = stream.getString();
    mPosition = stream.getBlockPosition();
    mSettings.read(stream);
    mOperation = (Operation) stream.getByte();
}

void StructureTemplateDataRequestPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
