#include "Falcon/Plugin/PluginBusEvents.h"

#include "Core/Event/EventBus.h"
#include "Core/Json/Json.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Falcon/Plugin/PluginPackets.h"
#include "Network/Crypto/Base64.h"
#include "Network/NetworkIdentifier.h"
#include "Protocol/Types/ItemStack.h"
#include "Protocol/Types/SerializedSkin.h"

#include <cstdint>
#include <string>
#include <utility>

namespace {
    std::string skinEventData(const SerializedSkin &skin) {
        std::string geometryName;
        if (auto patch = json::parse(skin.mSkinResourcePatch)) {
            const json::Value *geometry = patch->get("geometry");
            const json::Value *entry = geometry == nullptr ? nullptr : geometry->get("default");
            if (entry != nullptr)
                geometryName = entry->string();
        }
        return "[\"" + Base64::encode(skin.mSkinId) + "\",\"" + Base64::encode(skin.mSkinData.mData) + "\",\""
               + Base64::encode(skin.mCapeData.mData) + "\",\"" + Base64::encode(geometryName) + "\",\""
               + Base64::encode(skin.mGeometryData) + "\"]";
    }

    bool applySkinEvent(const std::string &payload, SerializedSkin &skin) {
        const auto data = json::parse(payload);
        if (data == nullptr || !data->isArray() || data->mArray.size() != 5)
            return false;
        for (const auto &value: data->mArray) {
            if (!value->isString())
                return false;
        }
        const auto &fields = data->mArray;
        std::string id;
        std::string image;
        std::string cape;
        std::string geometryName;
        std::string geometry;
        if (fields[0]->mString.size() > 44'000 || fields[1]->mString.size() > 90'000
            || fields[2]->mString.size() > 11'000 || fields[3]->mString.size() > 44'000
            || fields[4]->mString.size() > 350'000 || !Base64::decode(fields[0]->mString, id) || id.empty()
            || id.size() > INT16_MAX || !Base64::decode(fields[1]->mString, image)
            || (!fields[2]->mString.empty() && !Base64::decode(fields[2]->mString, cape))
            || (!fields[3]->mString.empty() && !Base64::decode(fields[3]->mString, geometryName))
            || (!fields[4]->mString.empty() && !Base64::decode(fields[4]->mString, geometry)))
            return false;
        int32_t width = 64;
        int32_t height = 64;
        if (image.size() == 64 * 32 * 4)
            height = 32;
        else if (image.size() == 128 * 128 * 4)
            width = height = 128;
        else if (image.size() != 64 * 64 * 4)
            return false;
        if (!cape.empty() && cape.size() != 64 * 32 * 4)
            return false;
        skin.mSkinId = id;
        skin.mFullSkinId = id;
        skin.mSkinData = SkinImageData{width, height, std::move(image)};
        const bool hasCape = !cape.empty();
        skin.mCapeData = SkinImageData{hasCape ? 64 : 0, hasCape ? 32 : 0, std::move(cape)};
        skin.mSkinResourcePatch = "{\"geometry\":{\"default\":\"" + json::escape(geometryName) + "\"}}";
        skin.mGeometryData = std::move(geometry);
        return true;
    }

    void hookServerBefore(PluginManager &plugins, ServerBeforeSignals &before) {
        plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_CONNECTION_OPEN), before.mConnectionOpen,
                        [&plugins](ConnectionOpenBeforeEvent &source) {
                            const bool rakNet = source.mId.getType() == NetworkIdentifier::Type::RakNet;
                            PluginEvent event;
                            event.mType = FALCON_EVENT_CONNECTION_OPEN;
                            event.mCancellable = true;
                            event.mCancelled = source.isCancelled();
                            event.mAddress = source.mId.getAddress();
                            event.mTransport = rakNet ? "raknet" : "nethernet";
                            event.mClientGuid = rakNet ? source.mId.getGuid().g : 0;
                            event.mMtuSize = source.mMtuSize;
                            plugins.dispatch(event);
                            source.setCancelled(event.mCancelled);
                        });

        plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_DATA_PACKET_RECEIVE), before.mDataPacketReceive,
                        [&plugins](DataPacketReceiveBeforeEvent &source) {
                            if (!PluginPackets::onReceive(plugins, source.mId, source.mData))
                                source.cancel();
                        });

        plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_SERVER_COMMAND), before.mServerCommand,
                        [&plugins](ServerCommandBeforeEvent &source) {
                            PluginEvent event;
                            event.mType = FALCON_EVENT_SERVER_COMMAND;
                            event.mCancellable = true;
                            event.mCancelled = source.isCancelled();
                            event.mMessage = &source.mCommandLine;
                            plugins.dispatch(event);
                            source.setCancelled(event.mCancelled);
                        });

        plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_COMMAND), before.mPlayerCommand,
                        [&plugins](PlayerCommandBeforeEvent &source) {
                            PluginEvent event;
                            event.mType = FALCON_EVENT_PLAYER_COMMAND;
                            event.mCancellable = true;
                            event.mCancelled = source.isCancelled();
                            event.mPlayer = &source.mPlayer;
                            event.mMessage = &source.mCommandLine;
                            plugins.dispatch(event);
                            source.setCancelled(event.mCancelled);
                        });

        plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_EMOTE), before.mPlayerEmote,
                        [&plugins](PlayerEmoteBeforeEvent &source) {
                            PluginEvent event;
                            event.mType = FALCON_EVENT_PLAYER_EMOTE;
                            event.mCancellable = true;
                            event.mCancelled = source.isCancelled();
                            event.mPlayer = &source.mPlayer;
                            event.mMessage = &source.mEmoteId;
                            plugins.dispatch(event);
                            source.setCancelled(event.mCancelled);
                        });

        plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_CHANGE_SKIN), before.mPlayerSkinChange,
                        [&plugins](PlayerSkinChangeBeforeEvent &source) {
                            const std::string oldData = skinEventData(source.mCurrentSkin);
                            const std::string proposed = skinEventData(source.mSkin);
                            PluginEvent event;
                            event.mType = FALCON_EVENT_PLAYER_CHANGE_SKIN;
                            event.mCancellable = true;
                            event.mCancelled = source.isCancelled();
                            event.mPlayer = &source.mPlayer;
                            event.mCustomData = "[" + oldData + "," + proposed + "]";
                            const std::string original = event.mCustomData;
                            plugins.dispatch(event);
                            // a rewritten skin the server can't read refuses the change
                            source.setCancelled(event.mCancelled
                                                || (event.mCustomData != original
                                                    && !applySkinEvent(event.mCustomData, source.mSkin)));
                        });

        plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_SIGN_CHANGE), before.mSignChange,
                        [&plugins](SignChangeBeforeEvent &source) {
                            PluginEvent event;
                            event.mType = FALCON_EVENT_SIGN_CHANGE;
                            event.mCancellable = true;
                            event.mCancelled = source.isCancelled();
                            event.mPlayer = &source.mPlayer;
                            event.mLevel = &source.mLevel;
                            event.mBlockPosition = source.mBlockPosition;
                            event.mState = source.mFrontSide;
                            event.mMessage = &source.mText;
                            plugins.dispatch(event);
                            source.setCancelled(event.mCancelled);
                        });

        plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_BLOCK_PICK), before.mPlayerBlockPick,
                        [&plugins](PlayerBlockPickBeforeEvent &source) {
                            PluginEvent event;
                            event.mType = FALCON_EVENT_PLAYER_BLOCK_PICK;
                            event.mCancellable = true;
                            event.mCancelled = source.isCancelled();
                            event.mPlayer = &source.mPlayer;
                            event.mLevel = &source.mLevel;
                            event.mBlockPosition = source.mBlockPosition;
                            event.mResult = &source.mPicked;
                            plugins.dispatch(event);
                            source.setCancelled(event.mCancelled);
                        });

        plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_ENTITY_PICK), before.mPlayerEntityPick,
                        [&plugins](PlayerEntityPickBeforeEvent &source) {
                            PluginEvent event;
                            event.mType = FALCON_EVENT_PLAYER_ENTITY_PICK;
                            event.mCancellable = true;
                            event.mCancelled = source.isCancelled();
                            event.mPlayer = &source.mPlayer;
                            event.mEntity = &source.mEntity;
                            event.mResult = &source.mPicked;
                            plugins.dispatch(event);
                            source.setCancelled(event.mCancelled);
                        });

        plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ITEM_SPAWN), before.mItemSpawn,
                        [&plugins](ItemSpawnBeforeEvent &source) {
                            ItemStack spawned = source.mItem;
                            PluginEvent event;
                            event.mType = FALCON_EVENT_ITEM_SPAWN;
                            event.mCancellable = true;
                            event.mCancelled = source.isCancelled();
                            event.mLevel = &source.mLevel;
                            event.mItem = &spawned;
                            event.mPosition = source.mPosition;
                            plugins.dispatch(event);
                            source.setCancelled(event.mCancelled);
                        });

        plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_GAME_MODE_CHANGE), before.mPlayerGameModeChange,
                        [&plugins](PlayerGameModeChangeBeforeEvent &source) {
                            PluginEvent event;
                            event.mType = FALCON_EVENT_PLAYER_GAME_MODE_CHANGE;
                            event.mCancellable = true;
                            event.mCancelled = source.isCancelled();
                            event.mPlayer = &source.mPlayer;
                            event.mGameMode = source.mGameMode;
                            event.mPreviousGameMode = source.mPreviousGameMode;
                            plugins.dispatch(event);
                            source.setCancelled(event.mCancelled);
                        });
    }

    void hookServerAfter(PluginManager &plugins, ServerAfterSignals &after) {
        plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_SERVER_TICK), after.mServerTick,
                        [&plugins](ServerTickAfterEvent &source) {
                            PluginEvent event;
                            event.mType = FALCON_EVENT_SERVER_TICK;
                            event.mTick = source.mTick;
                            plugins.dispatch(event);
                        });

        plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_UPDATE_NOTIFY), after.mUpdateNotify,
                        [&plugins](UpdateNotifyAfterEvent &source) {
                            PluginEvent event;
                            event.mType = FALCON_EVENT_UPDATE_NOTIFY;
                            event.mCustomData = source.mRelease;
                            plugins.dispatch(event);
                        });

        plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_WORLD_DIFFICULTY_CHANGE), after.mDifficultyChange,
                        [&plugins](DifficultyChangeAfterEvent &source) {
                            PluginEvent event;
                            event.mType = FALCON_EVENT_WORLD_DIFFICULTY_CHANGE;
                            event.mLevel = &source.mLevel;
                            event.mPreviousAmount = (int) source.mPreviousDifficulty;
                            event.mAmount = (int) source.mDifficulty;
                            plugins.dispatch(event);
                        });

        plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_SPAWN_CHANGE), after.mWorldSpawnChange,
                        [&plugins](WorldSpawnChangeAfterEvent &source) {
                            const Vector3i &previous = source.mPreviousSpawn;
                            PluginEvent event;
                            event.mType = FALCON_EVENT_SPAWN_CHANGE;
                            event.mLevel = &source.mLevel;
                            event.mFrom = Vector3f((float) previous.x, (float) previous.y, (float) previous.z);
                            plugins.dispatch(event);
                        });

        plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_VIEW_DISTANCE_CHANGE),
                        after.mPlayerViewDistanceChange, [&plugins](PlayerViewDistanceChangeAfterEvent &source) {
                            PluginEvent event;
                            event.mType = FALCON_EVENT_PLAYER_VIEW_DISTANCE_CHANGE;
                            event.mPlayer = &source.mPlayer;
                            // -1: no distance before the first request
                            event.mPreviousAmount = source.mPreviousDistance == 0 ? -1 : source.mPreviousDistance;
                            event.mAmount = source.mDistance;
                            plugins.dispatch(event);
                        });

        plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_BED_LEAVE), after.mPlayerBedLeave,
                        [&plugins](PlayerBedLeaveAfterEvent &source) {
                            PluginEvent event;
                            event.mType = FALCON_EVENT_PLAYER_BED_LEAVE;
                            event.mPlayer = &source.mPlayer;
                            event.mBlockPosition = source.mBedPosition;
                            plugins.dispatch(event);
                        });
    }
}

void PluginBusEvents::hookServer(PluginManager &plugins, EventBus &bus) {
    hookServerBefore(plugins, bus.before());
    hookServerAfter(plugins, bus.after());
}
