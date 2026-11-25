#include "Protocol/Packets/TextPacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {

    enum MessageCategory : unsigned char {
        MessageOnly = 0,
        AuthorAndMessage = 1,
        MessageAndParams = 2
    };

}

TextPacket::TextPacket()
        : mType(Type::Raw), mNeedsTranslation(false) {}

void TextPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putBool(mNeedsTranslation);

    const std::string message = mMessage.empty() ? " " : mMessage;

    switch (mType) {
        case Type::Chat:
        case Type::Whisper:
        case Type::Announcement:
            stream.putByte(AuthorAndMessage);
            stream.putByte((unsigned char) mType);
            stream.putString(mSourceName);
            stream.putString(message);
            break;

        case Type::Translation:
        case Type::Popup:
        case Type::JukeboxPopup:
            stream.putByte(MessageAndParams);
            stream.putByte((unsigned char) mType);
            stream.putString(message);
            stream.putArrayLength((uint32_t) mParameters.size());
            for (const std::string &parameter: mParameters)
                stream.putString(parameter);
            break;

        default:
            stream.putByte(MessageOnly);
            stream.putByte((unsigned char) mType);
            stream.putString(message);
            break;
    }

    stream.putString(mXuid);
    stream.putString(mPlatformChatId);

    stream.putOptionalPresent(!mFilteredMessage.empty());
    if (!mFilteredMessage.empty())
        stream.putString(mFilteredMessage);
}

void TextPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mNeedsTranslation = stream.getBool();

    const unsigned char category = stream.getByte();

    switch (category) {
        case MessageOnly:
            mType = (Type) stream.getByte();
            mMessage = stream.getString();
            break;

        case AuthorAndMessage:
            mType = (Type) stream.getByte();
            mSourceName = stream.getString();
            mMessage = stream.getString();
            break;

        case MessageAndParams: {
            mType = (Type) stream.getByte();
            mMessage = stream.getString();

            const uint32_t parameterCount = stream.getArrayLength();
            mParameters.clear();
            mParameters.reserve(parameterCount);

            for (uint32_t i = 0; i < parameterCount; i++)
                mParameters.push_back(stream.getString());
            break;
        }

        default:
            throw BinaryDataException("Not oneOf<MessageOnly, AuthorAndMessage, MessageAndParams>");
    }

    mXuid = stream.getString();
    mPlatformChatId = stream.getString();

    if (stream.getOptionalPresent())
        mFilteredMessage = stream.getString();
}

void TextPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
