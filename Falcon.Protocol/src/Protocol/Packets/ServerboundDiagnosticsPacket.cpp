#include "Protocol/Packets/ServerboundDiagnosticsPacket.h"

#include "Protocol/NetworkPacketHandler.h"

ServerboundDiagnosticsPacket::ServerboundDiagnosticsPacket() = default;

void ServerboundDiagnosticsPacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putLFloat(mAvgFps);
    stream.putLFloat(mAvgServerSimTickTimeMs);
    stream.putLFloat(mAvgClientSimTickTimeMs);
    stream.putLFloat(mAvgBeginFrameTimeMs);
    stream.putLFloat(mAvgInputTimeMs);
    stream.putLFloat(mAvgRenderTimeMs);
    stream.putLFloat(mAvgEndFrameTimeMs);
    stream.putLFloat(mAvgRemainderTimePercent);
    stream.putLFloat(mAvgUnaccountedTimePercent);

    stream.putArrayLength((uint32_t) mMemoryCategoryValues.size());
    for (const MemoryCategoryCounter &counter: mMemoryCategoryValues) {
        stream.putByte((unsigned char) counter.mCategory);
        stream.putLLong((uint64_t) counter.mCurrentBytes);
    }

    stream.putArrayLength((uint32_t) mEntityDiagnostics.size());
    for (const EntityDiagnosticTimingInfo &info: mEntityDiagnostics) {
        stream.putString(info.mDisplayName);
        stream.putString(info.mEntity);
        stream.putLLong((uint64_t) info.mTimeInNs);
        stream.putByte((unsigned char) info.mPercentOfTotal);
        stream.putVector3f(info.mPosition);
        stream.putString(info.mDimension);
    }

    stream.putArrayLength((uint32_t) mSystemDiagnostics.size());
    for (const SystemDiagnosticTimingInfo &info: mSystemDiagnostics) {
        stream.putString(info.mDisplayName);
        stream.putLLong((uint64_t) info.mSystemIndex);
        stream.putLLong((uint64_t) info.mTimeInNs);
        stream.putByte((unsigned char) info.mPercentOfTotal);
    }

    stream.putArrayLength((uint32_t) mSystemCategories.size());
    for (const SystemCategory &info: mSystemCategories) {
        stream.putString(info.mCategoryName);
        stream.putLLong((uint64_t) info.mSystemIndex);
    }

    stream.putArrayLength((uint32_t) mWhiskerScopes.size());
    for (const WhiskerScopeDataSummary &info: mWhiskerScopes) {
        stream.putString(info.mLabel);
        stream.putString(info.mIndentation);
        stream.putLLong((uint64_t) info.mTotalHighCostNs);
        stream.putLLong((uint64_t) info.mTotalMidCostNs);
        stream.putLLong((uint64_t) info.mTotalLowCostNs);
    }
}

void ServerboundDiagnosticsPacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mAvgFps = stream.getLFloat();
    mAvgServerSimTickTimeMs = stream.getLFloat();
    mAvgClientSimTickTimeMs = stream.getLFloat();
    mAvgBeginFrameTimeMs = stream.getLFloat();
    mAvgInputTimeMs = stream.getLFloat();
    mAvgRenderTimeMs = stream.getLFloat();
    mAvgEndFrameTimeMs = stream.getLFloat();
    mAvgRemainderTimePercent = stream.getLFloat();
    mAvgUnaccountedTimePercent = stream.getLFloat();

    uint32_t memoryCount = stream.getArrayLength();
    mMemoryCategoryValues.reserve(memoryCount);
    for (uint32_t i = 0; i < memoryCount; i++) {
        MemoryCategoryCounter counter;
        counter.mCategory = stream.getByte();
        counter.mCurrentBytes = (int64_t) stream.getLLong();
        mMemoryCategoryValues.push_back(counter);
    }

    uint32_t entityCount = stream.getArrayLength();
    mEntityDiagnostics.reserve(entityCount);
    for (uint32_t i = 0; i < entityCount; i++) {
        EntityDiagnosticTimingInfo info;
        info.mDisplayName = stream.getString();
        info.mEntity = stream.getString();
        info.mTimeInNs = (int64_t) stream.getLLong();
        info.mPercentOfTotal = (int8_t) stream.getByte();
        info.mPosition = stream.getVector3f();
        info.mDimension = stream.getString();
        mEntityDiagnostics.push_back(info);
    }

    uint32_t systemCount = stream.getArrayLength();
    mSystemDiagnostics.reserve(systemCount);
    for (uint32_t i = 0; i < systemCount; i++) {
        SystemDiagnosticTimingInfo info;
        info.mDisplayName = stream.getString();
        info.mSystemIndex = (int64_t) stream.getLLong();
        info.mTimeInNs = (int64_t) stream.getLLong();
        info.mPercentOfTotal = (int8_t) stream.getByte();
        mSystemDiagnostics.push_back(info);
    }

    uint32_t categoryCount = stream.getArrayLength();
    mSystemCategories.reserve(categoryCount);
    for (uint32_t i = 0; i < categoryCount; i++) {
        SystemCategory info;
        info.mCategoryName = stream.getString();
        info.mSystemIndex = (int64_t) stream.getLLong();
        mSystemCategories.push_back(info);
    }

    uint32_t whiskerCount = stream.getArrayLength();
    mWhiskerScopes.reserve(whiskerCount);
    for (uint32_t i = 0; i < whiskerCount; i++) {
        WhiskerScopeDataSummary info;
        info.mLabel = stream.getString();
        info.mIndentation = stream.getString();
        info.mTotalHighCostNs = (int64_t) stream.getLLong();
        info.mTotalMidCostNs = (int64_t) stream.getLLong();
        info.mTotalLowCostNs = (int64_t) stream.getLLong();
        mWhiskerScopes.push_back(info);
    }
}

void ServerboundDiagnosticsPacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
