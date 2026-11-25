#include "Falcon/Plugin/PluginBusEvents.h"

#include "Core/Event/EventBus.h"
#include "Core/Json/Json.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Falcon/Plugin/PluginWorldEventData.h"
#include "Network/ConnectionRequest.h"
#include "Network/Crypto/Base64.h"
#include "Network/NetworkIdentifier.h"
#include "Resources/ResourcePackManager.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockState.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {
    void hookToggle(PluginManager &plugins, FalconEventType type, EventSignal<PlayerToggleBeforeEvent> &signal) {
        plugins.hookBus(plugins.hasSubscribers(type), signal, [&plugins, type](PlayerToggleBeforeEvent &source) {
            PluginEvent event;
            event.mType = type;
            event.mCancellable = true;
            event.mCancelled = source.isCancelled();
            event.mPlayer = &source.mPlayer;
            event.mState = source.mActive;
            plugins.dispatch(event);
            source.setCancelled(event.mCancelled);
        });
    }
}

void PluginBusEvents::hookPlayer(PluginManager &plugins, EventBus &bus) {
    PlayerBeforeSignals &before = bus.before();
    PlayerAfterSignals &after = bus.after();

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_LOGIN_ATTEMPT), before.mPlayerLoginAttempt,
                    [&plugins](PlayerLoginAttemptBeforeEvent &source) {
                        const ConnectionRequest &request = source.mRequest;
                        std::string clientData = request.getClientData();
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_LOGIN_ATTEMPT;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mMessage = &source.mKickReason;
                        event.mState = source.mRequireAuthentication;
                        event.mPreviousAmount = source.mClientPort;
                        event.mAmount = source.mRequireAuthentication ? 1.0 : 0.0;
                        event.mPacketData = &clientData;
                        const SerializedSkin &skin = request.getSkin();
                        event.mCustomData = "[\"" + Base64::encode(request.getDisplayName()) + "\",\""
                                            + Base64::encode(request.getIdentity()) + "\",\""
                                            + Base64::encode(request.getXuid()) + "\",\""
                                            + Base64::encode(request.getLanguageCode()) + "\",\""
                                            + Base64::encode(source.mSource.getAddress()) + "\",\""
                                            + Base64::encode(skin.mSkinId) + "\",\""
                                            + Base64::encode(skin.mSkinData.mData) + "\"]";
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                        // a plugin may require authentication, never waive it
                        source.mRequireAuthentication = source.mRequireAuthentication || event.mAmount != 0.0;
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_PRE_LOGIN), before.mPlayerPreLogin,
                    [&plugins](PlayerPreLoginBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_PRE_LOGIN;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mMessage = &source.mKickReason;
                        event.mAmount = source.mClientPort;
                        event.mClientData = &source.mClientData;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_DUPLICATE_LOGIN), before.mPlayerDuplicateLogin,
                    [&plugins](PlayerDuplicateLoginBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_DUPLICATE_LOGIN;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mTarget = &source.mExisting;
                        event.mMessage = &source.mKickReason;
                        event.mState = source.mSameAccount;
                        event.mAmount = source.mClientPort;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_RESOURCE_PACK_OFFER), before.mPlayerResourcePackOffer,
                    [&plugins](PlayerResourcePackOfferBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_RESOURCE_PACK_OFFER;
                        event.mPlayer = &source.mPlayer;
                        event.mState = source.mOffer->mRequired;
                        event.mResourcePackOffer = &source.mOffer;
                        plugins.dispatch(event);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_CREATION), after.mPlayerCreation,
                    [&plugins](PlayerCreationAfterEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_CREATION;
                        event.mPlayer = &source.mPlayer;
                        event.mAmount = source.mClientPort;
                        plugins.dispatch(event);
                    });

    hookToggle(plugins, FALCON_EVENT_PLAYER_TOGGLE_SPRINT, before.mPlayerToggleSprint);
    hookToggle(plugins, FALCON_EVENT_PLAYER_TOGGLE_SWIM, before.mPlayerToggleSwim);
    hookToggle(plugins, FALCON_EVENT_PLAYER_TOGGLE_GLIDE, before.mPlayerToggleGlide);
    hookToggle(plugins, FALCON_EVENT_PLAYER_TOGGLE_FLIGHT, before.mPlayerToggleFlight);

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_TOGGLE_SNEAK), before.mPlayerToggleSneak,
                    [&plugins](PlayerToggleSneakBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_TOGGLE_SNEAK;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mState = source.mSneaking;
                        event.mAmount = source.mPressed ? 1 : -1;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_MISS_SWING), before.mPlayerMissSwing,
                    [&plugins](PlayerMissSwingBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_MISS_SWING;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mState = true;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_JUMP), after.mPlayerJump,
                    [&plugins](PlayerJumpAfterEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_JUMP;
                        event.mPlayer = &source.mPlayer;
                        event.mState = true;
                        plugins.dispatch(event);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_MOVE), before.mPlayerMove,
                    [&plugins](PlayerMoveBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_MOVE;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mFrom = source.mFrom;
                        event.mTo = source.mTo;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                        if (event.mToChanged) {
                            source.mTo = event.mTo;
                            source.mToChanged = true;
                        }
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ENTITY_FROST_WALKER), before.mPlayerFrostWalker,
                    [&plugins](PlayerFrostWalkerBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ENTITY_FROST_WALKER;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mEntity = &source.mPlayer;
                        event.mLevel = &source.mLevel;
                        event.mAmount = source.mRadius;
                        event.mBlockPosition = source.mCenter;
                        const std::string original = "{\"liquid\":" + PluginWorldEventData::encodeBlock(source.mLiquid)
                                                     + ",\"target\":"
                                                     + PluginWorldEventData::encodeBlock(source.mFrozen) + "}";
                        event.mCustomData = original;
                        plugins.dispatch(event);
                        if (event.mCancelled || !std::isfinite(event.mAmount) || event.mAmount < 0) {
                            source.cancel();
                            return;
                        }
                        source.mRadius = (int32_t) std::min(event.mAmount, 64.0);
                        if (event.mCustomData == original)
                            return;

                        const auto data = json::parse(event.mCustomData);
                        const auto *liquidData = data != nullptr ? data->get("liquid") : nullptr;
                        const auto *targetData = data != nullptr ? data->get("target") : nullptr;
                        if (liquidData == nullptr || targetData == nullptr
                            || !PluginWorldEventData::decodeBlock(*liquidData, source.mLiquid)
                            || !PluginWorldEventData::decodeBlock(*targetData, source.mFrozen)) {
                            source.cancel();
                            return;
                        }
                        source.mExactLiquid = true;
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_BLOCK_BREAK), before.mPlayerDestroyBlock,
                    [&plugins](PlayerDestroyBlockBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_BLOCK_BREAK;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mBlockName = source.mBlock.mName;
                        event.mDrops = &source.mDrops;
                        event.mExperience = &source.mExperience;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_INTERACT_BLOCK), before.mPlayerInteractBlock,
                    [&plugins](PlayerInteractBlockBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_INTERACT_BLOCK;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mBlockFace = (uint32_t) source.mFace;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_BLOCK_PLACE), before.mPlayerPlaceBlock,
                    [&plugins](PlayerPlaceBlockBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_BLOCK_PLACE;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mBlockName = source.mBlock.mName;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_POST_CHUNK_SEND), after.mPlayerChunkSent,
                    [&plugins](PlayerChunkSentAfterEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_POST_CHUNK_SEND;
                        event.mPlayer = &source.mPlayer;
                        event.mLevel = &source.mLevel;
                        event.mChunkX = source.mChunkX;
                        event.mChunkZ = source.mChunkZ;
                        plugins.dispatch(event);
                    });
}
