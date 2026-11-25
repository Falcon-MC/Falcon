#include "Protocol/Packets/LecternUpdatePacket.h"

#include "Protocol/NetworkPacketHandler.h"

void LecternUpdatePacket::write(BinaryStream& stream, const PacketCodecContext& context) const
{
    stream.putByte(mPage);
    stream.putByte(mTotalPages);
    stream.putVector3i(mBlockPosition);
}

void LecternUpdatePacket::read(ReadOnlyBinaryStream& stream, const PacketCodecContext& context)
{
    mPage = stream.getByte();
    mTotalPages = stream.getByte();
    mBlockPosition = stream.getVector3i();
}

void LecternUpdatePacket::handle(const NetworkIdentifier& id, NetworkPacketHandler& handler) const
{
    handler.handle(id, *this);
}
