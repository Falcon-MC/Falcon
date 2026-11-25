#include "Protocol/Packets/ChatPacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {

    void writePart(BinaryStream &stream, const ChatFormatting::Part &part) {
        stream.putOptionalPresent(part.mPresent);
        if (part.mPresent)
            stream.putString(part.mValue);
    }

    ChatFormatting::Part readPart(ReadOnlyBinaryStream &stream) {
        ChatFormatting::Part part;
        part.mPresent = stream.getOptionalPresent();
        if (part.mPresent)
            part.mValue = stream.getString();
        return part;
    }

}

ChatPacket::ChatPacket() = default;

void ChatPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    // the signed block is relayed byte for byte: receivers check the signature over it
    stream.putString(mSignedChat.mKeyId);
    stream.putString(mSignedChat.mSenderId);
    stream.putString(mSignedChat.mMessageId);
    stream.putString(mSignedChat.mScanText);
    stream.putString(mSignedChat.mRedactedText);
    stream.putString(mSignedChat.mAuthorName);
    stream.putString(mSignedChat.mSignature);

    // outside the signature: the hosting server picks the room
    stream.putOptionalPresent(mHasRoomId);
    if (mHasRoomId)
        stream.putString(mRoomId);

    stream.putOptionalPresent(mHasFormatting);
    if (!mHasFormatting)
        return;

    writePart(stream, mFormatting.mNamePrefix);
    writePart(stream, mFormatting.mNamePostfix);
    writePart(stream, mFormatting.mMessagePrefix);
    writePart(stream, mFormatting.mMessagePostfix);
}

void ChatPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mSignedChat.mKeyId = stream.getString();
    mSignedChat.mSenderId = stream.getString();
    mSignedChat.mMessageId = stream.getString();
    mSignedChat.mScanText = stream.getString();
    mSignedChat.mRedactedText = stream.getString();
    mSignedChat.mAuthorName = stream.getString();
    mSignedChat.mSignature = stream.getString();

    mHasRoomId = stream.getOptionalPresent();
    mRoomId = mHasRoomId ? stream.getString() : std::string();

    mFormatting = ChatFormatting();
    mHasFormatting = stream.getOptionalPresent();
    if (!mHasFormatting)
        return;

    mFormatting.mNamePrefix = readPart(stream);
    mFormatting.mNamePostfix = readPart(stream);
    mFormatting.mMessagePrefix = readPart(stream);
    mFormatting.mMessagePostfix = readPart(stream);
}

void ChatPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
