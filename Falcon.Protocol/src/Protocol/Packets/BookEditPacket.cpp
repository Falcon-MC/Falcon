#include "Protocol/Packets/BookEditPacket.h"

#include "Protocol/NetworkPacketHandler.h"

BookEditPacket::BookEditPacket() = default;

void BookEditPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarInt(mInventorySlot);
    stream.putUnsignedVarInt((uint32_t) mAction);

    switch (mAction) {
        case Action::ReplacePage:
        case Action::AddPage:
            stream.putVarInt(mPageNumber);
            stream.putString(mText);
            stream.putString(mPhotoName);
            break;
        case Action::DeletePage:
            stream.putVarInt(mPageNumber);
            break;
        case Action::SwapPages:
            stream.putVarInt(mPageNumber);
            stream.putVarInt(mSecondaryPageNumber);
            break;
        case Action::SignBook:
            stream.putString(mTitle);
            stream.putString(mAuthor);
            stream.putString(mXuid);
            break;
    }
}

void BookEditPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mInventorySlot = stream.getVarInt();
    mAction = (Action) stream.getUnsignedVarInt();

    switch (mAction) {
        case Action::ReplacePage:
        case Action::AddPage:
            mPageNumber = stream.getVarInt();
            mText = stream.getString();
            mPhotoName = stream.getString();
            break;
        case Action::DeletePage:
            mPageNumber = stream.getVarInt();
            break;
        case Action::SwapPages:
            mPageNumber = stream.getVarInt();
            mSecondaryPageNumber = stream.getVarInt();
            break;
        case Action::SignBook:
            mTitle = stream.getString();
            mAuthor = stream.getString();
            mXuid = stream.getString();
            break;
    }
}

void BookEditPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
