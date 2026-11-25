#pragma once

#include "Core/Math/Vector3i.h"

#include <cstdint>

enum class PlayerActionType {
    StartBreak,
    AbortBreak,
    StopBreak,
    GetUpdatedBlock,
    DropItem,
    StartSleep,
    StopSleep,
    Respawn,
    Jump,
    StartSprint,
    StopSprint,
    StartSneak,
    StopSneak,
    DimensionChangeRequestOrCreativeDestroyBlock,
    DimensionChangeSuccess,
    StartGlide,
    StopGlide,
    BuildDenied,
    ContinueBreak,
    ChangeSkin,
    SetEnchantmentSeed,
    StartSwimming,
    StopSwimming,
    StartSpinAttack,
    StopSpinAttack,
    BlockInteract,
    BlockPredictDestroy,
    BlockContinueDestroy,
    StartItemUseOn,
    StopItemUseOn,
    HandledTeleport,
    MissedSwing,
    StartCrawling,
    StopCrawling,
    StartFlying,
    StopFlying,
    ReceivedServerData,
    StartUsingItem,
    InternalUpdate,
};

class PlayerBlockActionData {
public:
    PlayerActionType mAction = PlayerActionType::StartBreak;
    Vector3i mBlockPosition;
    int32_t mFace = 0;
};
