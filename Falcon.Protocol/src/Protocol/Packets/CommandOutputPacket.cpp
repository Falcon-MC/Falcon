#include "Protocol/Packets/CommandOutputPacket.h"

#include "Protocol/NetworkPacketHandler.h"

#include <array>

namespace {
    const std::array<std::string, 5> OUTPUT_TYPE_NAMES = {
        "none", "lastoutput", "silent", "alloutput", "dataset"
    };
}

CommandOutputPacket::CommandOutputPacket() = default;

void CommandOutputPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    mOrigin.write(stream);
    stream.putString(OUTPUT_TYPE_NAMES[(size_t) mType]);
    stream.putLInt((uint32_t) mSuccessCount);

    stream.putArrayLength((uint32_t) mMessages.size());
    for (const CommandOutputMessage &message: mMessages) {
        stream.putString(message.mMessageId);
        stream.putBool(message.mInternal);

        stream.putArrayLength((uint32_t) message.mParameters.size());
        for (const std::string &parameter: message.mParameters)
            stream.putString(parameter);
    }

    stream.putOptionalPresent(mHasData);
    if (mHasData)
        stream.putString(mData);
}

void CommandOutputPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mOrigin.read(stream);

    const std::string typeName = stream.getString();
    for (size_t i = 0; i < OUTPUT_TYPE_NAMES.size(); i++) {
        if (OUTPUT_TYPE_NAMES[i] == typeName) {
            mType = (CommandOutputType) i;
            break;
        }
    }

    mSuccessCount = (int32_t) stream.getLInt();

    const uint32_t messageCount = stream.getArrayLength();
    mMessages.clear();
    mMessages.reserve(messageCount);

    for (uint32_t i = 0; i < messageCount; i++) {
        CommandOutputMessage message;
        message.mMessageId = stream.getString();
        message.mInternal = stream.getBool();

        const uint32_t parameterCount = stream.getArrayLength();
        message.mParameters.reserve(parameterCount);
        for (uint32_t j = 0; j < parameterCount; j++)
            message.mParameters.push_back(stream.getString());

        mMessages.push_back(std::move(message));
    }

    mHasData = stream.getOptionalPresent();
    if (mHasData)
        mData = stream.getString();
}

void CommandOutputPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
