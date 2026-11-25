#pragma once

#include "Core/Event/Event.h"
#include "Core/Event/EventSignal.h"
#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdint>
#include <string>

class Actor;
class Level;
class NetworkIdentifier;
class ServerPlayer;
class SerializedSkin;
enum class Difficulty : int;

// a new connection; cancelling closes it
struct ConnectionOpenBeforeEvent : public CancellableEvent {
    const NetworkIdentifier &mId;
    uint16_t mMtuSize;

    ConnectionOpenBeforeEvent(const NetworkIdentifier &id, uint16_t mtuSize) : mId(id), mMtuSize(mtuSize) {}
};

// a received packet before it is parsed; cancelling drops it
struct DataPacketReceiveBeforeEvent : public CancellableEvent {
    const NetworkIdentifier &mId;
    std::string &mData;

    DataPacketReceiveBeforeEvent(const NetworkIdentifier &id, std::string &data) : mId(id), mData(data) {}
};

// a console command before it runs; cancelling drops it
struct ServerCommandBeforeEvent : public CancellableEvent {
    std::string &mCommandLine;

    explicit ServerCommandBeforeEvent(std::string &commandLine) : mCommandLine(commandLine) {}
};

// a player's command before it runs; cancelling drops it
struct PlayerCommandBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    std::string &mCommandLine;

    PlayerCommandBeforeEvent(ServerPlayer &player, std::string &commandLine)
            : mPlayer(player), mCommandLine(commandLine) {}
};

// an emote before the others see it; cancelling hides it
struct PlayerEmoteBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    std::string &mEmoteId;

    PlayerEmoteBeforeEvent(ServerPlayer &player, std::string &emoteId) : mPlayer(player), mEmoteId(emoteId) {}
};

// a new skin before the player takes it; cancelling keeps the current one
struct PlayerSkinChangeBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    const SerializedSkin &mCurrentSkin;
    SerializedSkin &mSkin;

    PlayerSkinChangeBeforeEvent(ServerPlayer &player, const SerializedSkin &currentSkin, SerializedSkin &skin)
            : mPlayer(player), mCurrentSkin(currentSkin), mSkin(skin) {}
};

// one side of a sign edit before it is written; cancelling refuses the whole edit
struct SignChangeBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    Level &mLevel;
    Vector3i mBlockPosition;
    bool mFrontSide;
    std::string &mText;

    SignChangeBeforeEvent(ServerPlayer &player, Level &level, const Vector3i &position, bool frontSide,
                          std::string &text)
            : mPlayer(player), mLevel(level), mBlockPosition(position), mFrontSide(frontSide), mText(text) {}
};

// a block pick before the item reaches the hotbar; cancelling gives nothing
struct PlayerBlockPickBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    Level &mLevel;
    Vector3i mBlockPosition;
    ItemStack &mPicked;

    PlayerBlockPickBeforeEvent(ServerPlayer &player, Level &level, const Vector3i &position, ItemStack &picked)
            : mPlayer(player), mLevel(level), mBlockPosition(position), mPicked(picked) {}
};

// an entity pick before the item reaches the hotbar; cancelling gives nothing
struct PlayerEntityPickBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    Actor &mEntity;
    ItemStack &mPicked;

    PlayerEntityPickBeforeEvent(ServerPlayer &player, Actor &entity, ItemStack &picked)
            : mPlayer(player), mEntity(entity), mPicked(picked) {}
};

// an item about to be dropped into the world; cancelling drops nothing
struct ItemSpawnBeforeEvent : public CancellableEvent {
    Level &mLevel;
    Vector3f mPosition;
    const ItemStack &mItem;

    ItemSpawnBeforeEvent(Level &level, const Vector3f &position, const ItemStack &item)
            : mLevel(level), mPosition(position), mItem(item) {}
};

// a game mode change before it applies; cancelling keeps the current mode
struct PlayerGameModeChangeBeforeEvent : public CancellableEvent {
    ServerPlayer &mPlayer;
    int32_t mPreviousGameMode;
    int32_t mGameMode;

    PlayerGameModeChangeBeforeEvent(ServerPlayer &player, int32_t previousGameMode, int32_t gameMode)
            : mPlayer(player), mPreviousGameMode(previousGameMode), mGameMode(gameMode) {}
};

// once per tick, before the network events run
struct ServerTickAfterEvent : public Event {
    uint64_t mTick;

    explicit ServerTickAfterEvent(uint64_t tick) : mTick(tick) {}
};

// a newer release found by the version check
struct UpdateNotifyAfterEvent : public Event {
    const std::string &mRelease;

    explicit UpdateNotifyAfterEvent(const std::string &release) : mRelease(release) {}
};

// once for every level, before the new difficulty is stored
struct DifficultyChangeAfterEvent : public Event {
    Level &mLevel;
    Difficulty mPreviousDifficulty;
    Difficulty mDifficulty;

    DifficultyChangeAfterEvent(Level &level, Difficulty previousDifficulty, Difficulty difficulty)
            : mLevel(level), mPreviousDifficulty(previousDifficulty), mDifficulty(difficulty) {}
};

struct WorldSpawnChangeAfterEvent : public Event {
    Level &mLevel;
    Vector3i mPreviousSpawn;

    WorldSpawnChangeAfterEvent(Level &level, const Vector3i &previousSpawn)
            : mLevel(level), mPreviousSpawn(previousSpawn) {}
};

struct PlayerViewDistanceChangeAfterEvent : public Event {
    ServerPlayer &mPlayer;
    // 0 before the player's first request
    int32_t mPreviousDistance;
    int32_t mDistance;

    PlayerViewDistanceChangeAfterEvent(ServerPlayer &player, int32_t previousDistance, int32_t distance)
            : mPlayer(player), mPreviousDistance(previousDistance), mDistance(distance) {}
};

// before the player stands up: still asleep in the bed
struct PlayerBedLeaveAfterEvent : public Event {
    ServerPlayer &mPlayer;
    Vector3i mBedPosition;

    PlayerBedLeaveAfterEvent(ServerPlayer &player, const Vector3i &bedPosition)
            : mPlayer(player), mBedPosition(bedPosition) {}
};

struct ServerBeforeSignals {
    EventSignal<ConnectionOpenBeforeEvent> mConnectionOpen;
    EventSignal<DataPacketReceiveBeforeEvent> mDataPacketReceive;
    EventSignal<ServerCommandBeforeEvent> mServerCommand;
    EventSignal<PlayerCommandBeforeEvent> mPlayerCommand;
    EventSignal<PlayerEmoteBeforeEvent> mPlayerEmote;
    EventSignal<PlayerSkinChangeBeforeEvent> mPlayerSkinChange;
    EventSignal<SignChangeBeforeEvent> mSignChange;
    EventSignal<PlayerBlockPickBeforeEvent> mPlayerBlockPick;
    EventSignal<PlayerEntityPickBeforeEvent> mPlayerEntityPick;
    EventSignal<ItemSpawnBeforeEvent> mItemSpawn;
    EventSignal<PlayerGameModeChangeBeforeEvent> mPlayerGameModeChange;

    void clearServerBeforeSignals() {
        mConnectionOpen.clear();
        mDataPacketReceive.clear();
        mServerCommand.clear();
        mPlayerCommand.clear();
        mPlayerEmote.clear();
        mPlayerSkinChange.clear();
        mSignChange.clear();
        mPlayerBlockPick.clear();
        mPlayerEntityPick.clear();
        mItemSpawn.clear();
        mPlayerGameModeChange.clear();
    }
};

struct ServerAfterSignals {
    EventSignal<ServerTickAfterEvent> mServerTick;
    EventSignal<UpdateNotifyAfterEvent> mUpdateNotify;
    EventSignal<DifficultyChangeAfterEvent> mDifficultyChange;
    EventSignal<WorldSpawnChangeAfterEvent> mWorldSpawnChange;
    EventSignal<PlayerViewDistanceChangeAfterEvent> mPlayerViewDistanceChange;
    EventSignal<PlayerBedLeaveAfterEvent> mPlayerBedLeave;

    void clearServerAfterSignals() {
        mServerTick.clear();
        mUpdateNotify.clear();
        mDifficultyChange.clear();
        mWorldSpawnChange.clear();
        mPlayerViewDistanceChange.clear();
        mPlayerBedLeave.clear();
    }
};
