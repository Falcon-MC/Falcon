#include "Protocol/Packets/SyncWorldClocksPacket.h"

#include "Protocol/NetworkPacketHandler.h"

namespace {

    void writeTimeMarker(BinaryStream &stream, const TimeMarkerData &marker) {
        stream.putUnsignedVarLong(marker.mId);
        stream.putString(marker.mName);
        stream.putVarInt(marker.mTime);
        stream.putOptionalPresent(marker.mHasPeriod);
        if (marker.mHasPeriod) {
            stream.putLInt((uint32_t) marker.mPeriod);
        }
    }

    TimeMarkerData readTimeMarker(ReadOnlyBinaryStream &stream) {
        TimeMarkerData marker;
        marker.mId = stream.getUnsignedVarLong();
        marker.mName = stream.getString();
        marker.mTime = stream.getVarInt();
        marker.mHasPeriod = stream.getOptionalPresent();
        if (marker.mHasPeriod) {
            marker.mPeriod = (int32_t) stream.getLInt();
        }
        return marker;
    }

}

SyncWorldClocksPacket::SyncWorldClocksPacket() = default;

void SyncWorldClocksPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putUnsignedVarInt((uint32_t) mType);

    switch (mType) {
        case SyncWorldClocksType::SyncState: {
            stream.putArrayLength((uint32_t) mClockStates.size());
            for (const SyncWorldClockStateData &state: mClockStates) {
                stream.putUnsignedVarLong(state.mClockId);
                stream.putVarInt(state.mTime);
                stream.putBool(state.mPaused);
            }
            break;
        }
        case SyncWorldClocksType::InitializeRegistry: {
            stream.putArrayLength((uint32_t) mClocks.size());
            for (const WorldClockData &clock: mClocks) {
                stream.putUnsignedVarLong(clock.mId);
                stream.putString(clock.mName);
                stream.putVarInt(clock.mTime);
                stream.putBool(clock.mPaused);

                stream.putArrayLength((uint32_t) clock.mTimeMarkers.size());
                for (const TimeMarkerData &marker: clock.mTimeMarkers) {
                    writeTimeMarker(stream, marker);
                }
            }
            break;
        }
        case SyncWorldClocksType::AddTimeMarker: {
            stream.putUnsignedVarLong(mAddClockId);
            stream.putArrayLength((uint32_t) mAddTimeMarkers.size());
            for (const TimeMarkerData &marker: mAddTimeMarkers) {
                writeTimeMarker(stream, marker);
            }
            break;
        }
        case SyncWorldClocksType::RemoveTimeMarker: {
            stream.putUnsignedVarLong(mRemoveClockId);
            stream.putArrayLength((uint32_t) mRemoveTimeMarkerIds.size());
            for (uint64_t markerId: mRemoveTimeMarkerIds) {
                stream.putUnsignedVarLong(markerId);
            }
            break;
        }
    }
}

void SyncWorldClocksPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mType = (SyncWorldClocksType) stream.getUnsignedVarInt();

    switch (mType) {
        case SyncWorldClocksType::SyncState: {
            uint32_t count = stream.getArrayLength();
            mClockStates.clear();
            mClockStates.reserve(count);
            for (uint32_t i = 0; i < count; i++) {
                SyncWorldClockStateData state;
                state.mClockId = stream.getUnsignedVarLong();
                state.mTime = stream.getVarInt();
                state.mPaused = stream.getBool();
                mClockStates.push_back(state);
            }
            break;
        }
        case SyncWorldClocksType::InitializeRegistry: {
            uint32_t count = stream.getArrayLength();
            mClocks.clear();
            mClocks.reserve(count);
            for (uint32_t i = 0; i < count; i++) {
                WorldClockData clock;
                clock.mId = stream.getUnsignedVarLong();
                clock.mName = stream.getString();
                clock.mTime = stream.getVarInt();
                clock.mPaused = stream.getBool();

                uint32_t markerCount = stream.getArrayLength();
                clock.mTimeMarkers.reserve(markerCount);
                for (uint32_t j = 0; j < markerCount; j++) {
                    clock.mTimeMarkers.push_back(readTimeMarker(stream));
                }

                mClocks.push_back(clock);
            }
            break;
        }
        case SyncWorldClocksType::AddTimeMarker: {
            mAddClockId = stream.getUnsignedVarLong();
            uint32_t count = stream.getArrayLength();
            mAddTimeMarkers.clear();
            mAddTimeMarkers.reserve(count);
            for (uint32_t i = 0; i < count; i++) {
                mAddTimeMarkers.push_back(readTimeMarker(stream));
            }
            break;
        }
        case SyncWorldClocksType::RemoveTimeMarker: {
            mRemoveClockId = stream.getUnsignedVarLong();
            uint32_t count = stream.getArrayLength();
            mRemoveTimeMarkerIds.clear();
            mRemoveTimeMarkerIds.reserve(count);
            for (uint32_t i = 0; i < count; i++) {
                mRemoveTimeMarkerIds.push_back(stream.getUnsignedVarLong());
            }
            break;
        }
    }
}

void SyncWorldClocksPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
