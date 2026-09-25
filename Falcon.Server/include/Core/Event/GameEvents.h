#pragma once

#include "Core/Event/Event.h"
#include "Core/Math/Vector3i.h"

#include <string>

class ServerPlayer;

struct PlayerChatBeforeEvent : public CancellableEvent {
    ServerPlayer &mSender;
    std::string mMessage;

    PlayerChatBeforeEvent(ServerPlayer &sender, std::string message)
            : mSender(sender), mMessage(std::move(message)) {}
};

struct PlayerChatAfterEvent : public Event {
    ServerPlayer &mSender;
    std::string mMessage;

    PlayerChatAfterEvent(ServerPlayer &sender, std::string message)
            : mSender(sender), mMessage(std::move(message)) {}
};

struct PlayerBreakBlockBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    Vector3i mBlockPosition;

    PlayerBreakBlockBeforeEvent(ServerPlayer &player, const Vector3i &position)
            : mPlayer(player), mBlockPosition(position) {}
};

struct PlayerBreakBlockAfterEvent : public Event {
    ServerPlayer &mPlayer;
    Vector3i mBlockPosition;
    std::string mBrokenBlockIdentifier;

    PlayerBreakBlockAfterEvent(ServerPlayer &player, const Vector3i &position, std::string identifier)
            : mPlayer(player), mBlockPosition(position), mBrokenBlockIdentifier(std::move(identifier)) {}
};

struct PlayerSpawnAfterEvent : public Event {
    ServerPlayer &mPlayer;
    bool mInitialSpawn;

    PlayerSpawnAfterEvent(ServerPlayer &player, bool initialSpawn)
            : mPlayer(player), mInitialSpawn(initialSpawn) {}
};

struct PlayerJoinAfterEvent : public Event {
    ServerPlayer &mPlayer;

    explicit PlayerJoinAfterEvent(ServerPlayer &player) : mPlayer(player) {}
};

struct PlayerLeaveAfterEvent : public Event {
    std::string mPlayerName;
    ServerPlayer *mPlayer;

    PlayerLeaveAfterEvent(std::string playerName, ServerPlayer *player)
            : mPlayerName(std::move(playerName)), mPlayer(player) {}
};

struct PlayerPlaceBlockAfterEvent : public Event {
    ServerPlayer &mPlayer;
    Vector3i mBlockPosition;
    std::string mBlockIdentifier;

    PlayerPlaceBlockAfterEvent(ServerPlayer &player, const Vector3i &position, std::string identifier)
            : mPlayer(player), mBlockPosition(position), mBlockIdentifier(std::move(identifier)) {}
};

struct PlayerGameModeChangeAfterEvent : public Event {
    ServerPlayer &mPlayer;
    int32_t mFromGameMode;
    int32_t mToGameMode;

    PlayerGameModeChangeAfterEvent(ServerPlayer &player, int32_t from, int32_t to)
            : mPlayer(player), mFromGameMode(from), mToGameMode(to) {}
};

struct EntityHurtAfterEvent : public Event {
    ServerPlayer &mHurtEntity;
    float mDamage;
    std::string mCause;

    EntityHurtAfterEvent(ServerPlayer &hurtEntity, float damage, std::string cause)
            : mHurtEntity(hurtEntity), mDamage(damage), mCause(std::move(cause)) {}
};

struct EntityDieAfterEvent : public Event {
    ServerPlayer &mDeadEntity;
    std::string mCause;

    EntityDieAfterEvent(ServerPlayer &deadEntity, std::string cause)
            : mDeadEntity(deadEntity), mCause(std::move(cause)) {}
};

struct ItemUseAfterEvent : public Event {
    ServerPlayer &mPlayer;
    std::string mItemIdentifier;

    ItemUseAfterEvent(ServerPlayer &player, std::string identifier)
            : mPlayer(player), mItemIdentifier(std::move(identifier)) {}
};

struct PlayerHotbarSelectedSlotChangeAfterEvent : public Event {
    ServerPlayer &mPlayer;
    int32_t mNewSlotSelected;
    int32_t mPreviousSlotSelected;
    std::string mItemIdentifier;

    PlayerHotbarSelectedSlotChangeAfterEvent(ServerPlayer &player, int32_t newSlot, int32_t previousSlot,
                                             std::string identifier)
            : mPlayer(player), mNewSlotSelected(newSlot), mPreviousSlotSelected(previousSlot),
              mItemIdentifier(std::move(identifier)) {}
};

struct EntityItemPickupAfterEvent : public Event {
    ServerPlayer &mEntity;
    std::string mItemIdentifier;
    int32_t mCount;

    EntityItemPickupAfterEvent(ServerPlayer &entity, std::string identifier, int32_t count)
            : mEntity(entity), mItemIdentifier(std::move(identifier)), mCount(count) {}
};

struct ItemStartUseAfterEvent : public Event {
    ServerPlayer &mSource;
    std::string mItemIdentifier;
    int32_t mUseDuration;

    ItemStartUseAfterEvent(ServerPlayer &source, std::string identifier, int32_t useDuration)
            : mSource(source), mItemIdentifier(std::move(identifier)), mUseDuration(useDuration) {}
};

struct ItemCompleteUseAfterEvent : public Event {
    ServerPlayer &mSource;
    std::string mItemIdentifier;
    int32_t mUseDuration;

    ItemCompleteUseAfterEvent(ServerPlayer &source, std::string identifier, int32_t useDuration)
            : mSource(source), mItemIdentifier(std::move(identifier)), mUseDuration(useDuration) {}
};

struct EffectAddAfterEvent : public Event {
    ServerPlayer &mEntity;
    int32_t mEffectId;
    int32_t mAmplifier;
    int32_t mDurationTicks;

    EffectAddAfterEvent(ServerPlayer &entity, int32_t effectId, int32_t amplifier, int32_t durationTicks)
            : mEntity(entity), mEffectId(effectId), mAmplifier(amplifier), mDurationTicks(durationTicks) {}
};
