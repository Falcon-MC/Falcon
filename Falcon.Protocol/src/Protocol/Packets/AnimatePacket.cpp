#include "Protocol/Packets/AnimatePacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {

    const struct {
        unsigned char id;
        AnimatePacket::Action action;
    } ACTION_IDS[] = {
            {0,   AnimatePacket::Action::NoAction},
            {1,   AnimatePacket::Action::SwingArm},
            {3,   AnimatePacket::Action::WakeUp},
            {4,   AnimatePacket::Action::CriticalHit},
            {5,   AnimatePacket::Action::MagicCriticalHit},
            {128, AnimatePacket::Action::RowRight},
            {129, AnimatePacket::Action::RowLeft}
    };

    unsigned char _toActionId(AnimatePacket::Action action) {
        for (const auto &entry: ACTION_IDS) {
            if (entry.action == action)
                return entry.id;
        }

        return 0;
    }

    AnimatePacket::Action _fromActionId(unsigned char id) {
        for (const auto &entry: ACTION_IDS) {
            if (entry.id == id)
                return entry.action;
        }

        throw BinaryDataException("Unknown animate action id");
    }

}

AnimatePacket::AnimatePacket()
        : mAction(Action::NoAction), mRuntimeActorId(0), mData(0.0f) {}

void AnimatePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putByte((unsigned char) _toActionId(mAction));
    stream.putUnsignedVarLong(mRuntimeActorId);
    stream.putLFloat(mData);

    stream.putOptionalPresent(!mSwingSource.empty());
    if (!mSwingSource.empty())
        stream.putString(mSwingSource);

    if (!context.getCapabilities().mAnimateHand)
        return;

    stream.putOptionalPresent(mHand.has_value());
    if (mHand.has_value())
        stream.putByte(*mHand);
}

void AnimatePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mAction = _fromActionId(stream.getByte());
    mRuntimeActorId = stream.getUnsignedVarLong();
    mData = stream.getLFloat();

    mSwingSource.clear();
    if (stream.getOptionalPresent())
        mSwingSource = stream.getString();

    mHand.reset();
    if (context.getCapabilities().mAnimateHand && stream.getOptionalPresent())
        mHand = stream.getByte();
}

void AnimatePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
