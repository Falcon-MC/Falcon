#pragma once

#include "Core/Math/Vector3f.h"
#include "Protocol/Packet.h"
#include "Protocol/Types/ItemUseTransaction.h"
#include "Protocol/Types/ItemStackRequest.h"
#include "Protocol/Types/PlayerBlockActionData.h"

#include <cstdint>
#include <vector>

enum class PlayerInputMode {
    Undefined,
    Mouse,
    Touch,
    Gamepad,
    MotionController,
};

enum class PlayerClientPlayMode {
    Normal,
    Teaser,
    Screen,
    Viewer,
    Reality,
    Placement,
    LivingRoom,
    ExitLevel,
    ExitLevelLivingRoom,
};

enum class PlayerInputInteractionModel {
    Touch,
    Crosshair,
    Classic,
};

enum class PlayerAuthInputData {
    Ascend,
    Descend,
    NorthJump,
    JumpDown,
    SprintDown,
    ChangeHeight,
    Jumping,
    AutoJumpingInWater,
    Sneaking,
    SneakDown,
    Up,
    Down,
    Left,
    Right,
    UpLeft,
    UpRight,
    WantUp,
    WantDown,
    WantDownSlow,
    WantUpSlow,
    Sprinting,
    AscendBlock,
    DescendBlock,
    SneakToggleDown,
    PersistSneak,
    StartSprinting,
    StopSprinting,
    StartSneaking,
    StopSneaking,
    StartSwimming,
    StopSwimming,
    StartJumping,
    StartGliding,
    StopGliding,
    PerformItemInteraction,
    PerformBlockActions,
    PerformItemStackRequest,
    HandleTeleport,
    Emoting,
    MissedSwing,
    StartCrawling,
    StopCrawling,
    StartFlying,
    StopFlying,
    ReceivedServerData,
    InClientPredictedInVehicle,
    PaddleLeft,
    PaddleRight,
    BlockBreakingDelayEnabled,
    HorizontalCollision,
    VerticalCollision,
    DownLeft,
    DownRight,
    StartUsingItem,
    CameraRelativeMovementEnabled,
    RotControlledByMoveDirection,
    StartSpinAttack,
    StopSpinAttack,
    HotbarOnlyTouch,
    JumpReleasedRaw,
    JumpPressedRaw,
    JumpCurrentRaw,
    SneakReleasedRaw,
    SneakPressedRaw,
    SneakCurrentRaw,
    InternalUpdate,
};

class PlayerAuthInputPacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::PlayerAuthInput;

    PlayerAuthInputPacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "PlayerAuthInputPacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    Vector3f mRotation;
    Vector3f mPosition;
    float mMotionX = 0.0f;
    float mMotionY = 0.0f;
    std::vector<int32_t> mInputData;
    PlayerInputMode mInputMode = PlayerInputMode::Mouse;
    PlayerClientPlayMode mPlayMode = PlayerClientPlayMode::Normal;
    int64_t mTick = 0;
    Vector3f mDelta;

    bool mHasItemUseTransaction = false;
    ItemUseTransaction mItemUseTransaction;

    bool mHasItemStackRequest = false;
    ItemStackRequest mItemStackRequest;

    std::vector<PlayerBlockActionData> mPlayerActions;

    PlayerInputInteractionModel mInputInteractionModel = PlayerInputInteractionModel::Crosshair;
    float mInteractRotationX = 0.0f;
    float mInteractRotationY = 0.0f;

    bool mHasVehicleRotation = false;
    float mVehicleRotationX = 0.0f;
    float mVehicleRotationY = 0.0f;

    bool mHasPredictedVehicle = false;
    int64_t mPredictedVehicle = 0;

    float mAnalogMoveVectorX = 0.0f;
    float mAnalogMoveVectorY = 0.0f;
    Vector3f mCameraOrientation;
    float mRawMoveVectorX = 0.0f;
    float mRawMoveVectorY = 0.0f;

    bool hasInputFlag(int32_t flag) const;
};
