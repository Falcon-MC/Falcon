#include "Protocol/Types/CommandOriginData.h"

void CommandOriginData::write(BinaryStream &stream) const {
    stream.putString("player");
    stream.putUuid(mUuid);
    stream.putString(mRequestId);
    stream.putLLong((uint64_t) mPlayerId);
}

void CommandOriginData::read(ReadOnlyBinaryStream &stream) {
    stream.getString();
    mOrigin = CommandOriginType::Player;
    mUuid = stream.getUuid();
    mRequestId = stream.getString();
    mPlayerId = (int64_t) stream.getLLong();
}
