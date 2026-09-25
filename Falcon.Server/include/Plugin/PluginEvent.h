#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <falcon/falcon_api.h>

#include <cstdint>
#include <string>
#include <vector>

class Actor;
class ItemStack;
class Level;
class ServerPlayer;

struct PluginEvent {
    FalconEventType mType = 0;
    bool mCancellable = false;
    bool mCancelled = false;
    bool mMonitor = false;
    ServerPlayer *mPlayer = nullptr;
    std::string *mMessage = nullptr;
    Actor *mEntity = nullptr;
    Actor *mAttacker = nullptr;
    Vector3i mBlockPosition;
    uint32_t mBlockFace = 0;
    std::string mBlockName;
    double mAmount = 0.0;
    std::string mCause;
    Vector3f mFrom;
    Vector3f mTo;
    bool mToChanged = false;
    ItemStack *mItem = nullptr;
    uint32_t mPacketId = 0;
    std::string *mPacketData = nullptr;
    Level *mLevel = nullptr;
    Actor *mTarget = nullptr;
    Vector3f mPosition;
    std::vector<Vector3i> *mBlocks = nullptr;
    int32_t mGameMode = 0;
    int32_t mPreviousGameMode = 0;
    uint32_t mDimension = 0;
    uint32_t mPreviousDimension = 0;
    uint64_t mTick = 0;
    std::string mSourceContainer;
    int32_t mSourceSlot = -1;
    std::string mDestinationContainer;
    int32_t mDestinationSlot = -1;
};
