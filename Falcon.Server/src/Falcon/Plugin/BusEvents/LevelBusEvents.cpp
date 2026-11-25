#include "Falcon/Plugin/PluginBusEvents.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Event/EventBus.h"
#include "Core/Json/Json.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/ItemStack.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Dimension/Dimension.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <exception>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
    void dispatchWorldEffect(PluginManager &plugins, FalconEventType type, WorldEffectBeforeEvent &source) {
        ServerNetworkHandler &owner = plugins.getOwner();
        std::string recipients = "[";
        for (const NetworkIdentifier &id: source.mViewers) {
            const auto found = owner.getPlayers().find(id);
            if (found == owner.getPlayers().end())
                continue;
            if (recipients.size() != 1)
                recipients += ',';
            recipients += std::to_string(found->second.getRuntimeId());
        }
        recipients += ']';

        BinaryStream stream;
        source.mPacket.write(stream, owner.getCodecContext());
        std::string payload = stream.getBuffer();
        PluginEvent event;
        event.mType = type;
        event.mCancellable = true;
        event.mCancelled = source.isCancelled();
        event.mLevel = &source.mLevel;
        event.mPosition = source.mPosition;
        event.mPacketId = (uint32_t) source.mPacket.getId();
        event.mPacketData = &payload;
        event.mCustomData = recipients;
        plugins.dispatch(event);
        source.setCancelled(event.mCancelled);
        if (event.mCancelled || event.mCustomData == recipients)
            return;

        const auto list = json::parse(event.mCustomData);
        if (list == nullptr || !list->isArray() || list->mArray.size() > owner.getPlayers().size())
            return;
        std::vector<NetworkIdentifier> &viewers = source.mViewers;
        viewers.clear();
        for (const auto &value: list->mArray) {
            const double id = value->number(-1);
            if (!std::isfinite(id) || id < 0 || id >= 0x1p64)
                continue;
            ServerPlayer *player = owner.getPlayerByRuntimeId((uint64_t) id);
            if (player == nullptr || !player->isSpawned() || !player->isIn(source.mLevel))
                continue;
            const auto &target = player->getNetworkIdentifier();
            if (std::find(viewers.begin(), viewers.end(), target) == viewers.end())
                viewers.push_back(target);
        }
    }

    void dispatchCombust(PluginManager &plugins, FalconEventType type, ActorCombustBeforeEvent &source) {
        PluginEvent event;
        event.mType = type;
        event.mCancellable = true;
        event.mCancelled = source.isCancelled();
        event.mEntity = &source.mVictim;
        event.mAttacker = source.mSource;
        event.mAmount = source.mDuration / 20.0;
        if (source.mBlockPosition) {
            event.mLevel = &plugins.getOwner().getLevelFor(source.mVictim);
            event.mBlockPosition = *source.mBlockPosition;
        }
        plugins.dispatch(event);
        source.setCancelled(event.mCancelled || !std::isfinite(event.mAmount) || event.mAmount <= 0);
        if (!source.isCancelled())
            source.mDuration = (int32_t) std::min(event.mAmount * 20, (double) std::numeric_limits<int32_t>::max());
    }

    std::optional<std::vector<MobEffectInstance>> decodeCloudEffects(const std::string &payload) {
        const auto data = json::parse(payload);
        const auto *list = data != nullptr ? data->get("effects") : nullptr;
        if (list == nullptr || !list->isArray() || list->mArray.size() > MAX_MOB_EFFECT_ID)
            return std::nullopt;

        std::vector<MobEffectInstance> effects;
        for (const auto &entry: list->mArray) {
            if (!entry->isArray() || entry->mArray.size() != 5)
                return std::nullopt;
            const auto &fields = entry->mArray;
            const double id = fields[0]->number(-1);
            const double duration = fields[1]->number(-2);
            const double amplifier = fields[2]->number(-1);
            if (!std::isfinite(id) || id < 1 || id > MAX_MOB_EFFECT_ID || !isKnownMobEffect((int32_t) id)
                || !std::isfinite(duration) || duration < -1 || duration > INT32_MAX || !std::isfinite(amplifier)
                || amplifier < 0 || amplifier > 255)
                return std::nullopt;
            MobEffectInstance effect;
            effect.mId = (MobEffectId) (int32_t) id;
            effect.mInfinite = duration == -1;
            effect.mDuration = effect.mInfinite ? -1 : (int32_t) duration;
            effect.mAmplifier = (int32_t) amplifier;
            effect.mParticles = fields[3]->boolean(true);
            effect.mAmbient = fields[4]->boolean(false);
            effects.push_back(effect);
        }
        return effects;
    }

    void dispatchWorldOpen(PluginManager &plugins, FalconEventType type, WorldOpenAfterEvent &source) {
        PluginEvent event;
        event.mType = type;
        event.mWorldName = source.mWorld.getName();
        event.mLevel = &source.mWorld.getOverworld();
        plugins.dispatch(event);
    }

    void dispatchWeather(PluginManager &plugins, FalconEventType type, WeatherChangeBeforeEvent &source) {
        PluginEvent event;
        event.mType = type;
        event.mCancellable = true;
        event.mCancelled = source.isCancelled();
        event.mLevel = &source.mLevel;
        event.mState = source.mState;
        plugins.dispatch(event);
        source.setCancelled(event.mCancelled);
    }

    void dispatchChunk(PluginManager &plugins, FalconEventType type, ChunkAfterEvent &source) {
        PluginEvent event;
        event.mType = type;
        event.mLevel = &source.mLevel;
        event.mChunkX = source.mChunkX;
        event.mChunkZ = source.mChunkZ;
        event.mState = source.mGenerated;
        plugins.dispatch(event);
    }
}

void PluginBusEvents::hookLevel(PluginManager &plugins, EventBus &bus) {
    LevelBeforeSignals &before = bus.before();
    LevelAfterSignals &after = bus.after();

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_WORLD_SOUND), before.mWorldSound,
                    [&plugins](WorldEffectBeforeEvent &source) {
                        dispatchWorldEffect(plugins, FALCON_EVENT_WORLD_SOUND, source);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_WORLD_PARTICLE), before.mWorldParticle,
                    [&plugins](WorldEffectBeforeEvent &source) {
                        dispatchWorldEffect(plugins, FALCON_EVENT_WORLD_PARTICLE, source);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_EXPLOSION), before.mExplosion,
                    [&plugins](ExplosionBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_EXPLOSION;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mEntity = const_cast<ServerActor *>(source.mSource);
                        event.mLevel = &source.mLevel;
                        event.mPosition = source.mCenter;
                        event.mAmount = source.mSize;
                        event.mBlocks = &source.mBlocks;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                        if (!source.isCancelled())
                            source.mSize = std::max(event.mAmount, 0.0);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ENTITY_PRE_EXPLODE), before.mActorExplode,
                    [&plugins](ActorExplodeBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ENTITY_PRE_EXPLODE;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mEntity = const_cast<ServerActor *>(&source.mSource);
                        event.mLevel = &source.mLevel;
                        event.mPosition = source.mCenter;
                        event.mAmount = source.mSize;
                        event.mState = source.mBreakBlocks;
                        event.mPreviousAmount = source.mFireChance;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled || !std::isfinite(event.mAmount) || event.mAmount <= 0);
                        if (source.isCancelled())
                            return;

                        source.mSize = std::min(event.mAmount, 128.0);
                        bool breakBlocks = event.mState;
                        double fireChance = event.mPreviousAmount;
                        if (!event.mCustomData.empty()) {
                            const auto data = json::parse(event.mCustomData);
                            if (data != nullptr) {
                                if (const auto *value = data->get("breakBlocks"))
                                    breakBlocks = value->boolean(breakBlocks);
                                if (const auto *value = data->get("fireChance"))
                                    fireChance = value->number(fireChance);
                            }
                        }
                        source.mBreakBlocks = breakBlocks;
                        source.mFireChance = std::isfinite(fireChance) ? std::clamp(fireChance, 0.0, 1.0) : 0;
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ENTITY_SPAWN), before.mActorSpawn,
                    [&plugins](ActorSpawnBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ENTITY_SPAWN;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mEntity = &source.mActor;
                        event.mLevel = &source.mLevel;
                        event.mPosition = source.mActor.getPosition();
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ENTITY_SHOOT_BOW), before.mPlayerShootBow,
                    [&plugins](PlayerShootBowBeforeEvent &source) {
                        ItemStack bow = source.mWeapon;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ENTITY_SHOOT_BOW;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mEntity = &source.mPlayer;
                        event.mTarget = &source.mProjectile;
                        event.mItem = &bow;
                        event.mAmount = source.mForce;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled || !std::isfinite(event.mAmount) || event.mAmount < 0);
                        if (source.isCancelled())
                            return;

                        // the runtime id of an actor shot in its place
                        if (!event.mCustomData.empty()) {
                            uint64_t replacementId = 0;
                            const char *end = event.mCustomData.data() + event.mCustomData.size();
                            const auto parsed = std::from_chars(event.mCustomData.data(), end, replacementId);
                            if (parsed.ec == std::errc() && parsed.ptr == end)
                                source.mReplacementRuntimeId = replacementId;
                        }
                        source.mForce = (float) std::min(event.mAmount, 128.0);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PROJECTILE_LAUNCH), before.mProjectileLaunch,
                    [&plugins](ProjectileLaunchBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PROJECTILE_LAUNCH;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mEntity = &source.mProjectile;
                        event.mAttacker = source.mShooter;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PROJECTILE_HIT), before.mProjectileHit,
                    [&plugins](ProjectileHitBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PROJECTILE_HIT;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mEntity = &source.mProjectile;
                        event.mTarget = source.mTarget;
                        event.mLevel = &source.mLevel;
                        event.mPosition = source.mPosition;
                        if (source.mBlockPosition) {
                            const Vector3i &block = *source.mBlockPosition;
                            event.mBlockPosition = block;
                            event.mBlockName = source.mLevel.getBlockState(block.x, block.y, block.z).mName;
                        }

                        for (auto &entry: plugins.getOwner().getPlayers()) {
                            if (entry.second.getUniqueId() == source.mProjectile.getOwnerUniqueId())
                                event.mAttacker = &entry.second;
                        }

                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ENTITY_COMBUST_BY_BLOCK), before.mActorCombustByBlock,
                    [&plugins](ActorCombustBeforeEvent &source) {
                        dispatchCombust(plugins, FALCON_EVENT_ENTITY_COMBUST_BY_BLOCK, source);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ENTITY_COMBUST_BY_ENTITY), before.mActorCombustByActor,
                    [&plugins](ActorCombustBeforeEvent &source) {
                        dispatchCombust(plugins, FALCON_EVENT_ENTITY_COMBUST_BY_ENTITY, source);
                    });

    plugins.hookBus(
            plugins.hasSubscribers(FALCON_EVENT_AREA_EFFECT_CLOUD_APPLY), before.mAreaEffectCloudApply,
            [&plugins](AreaEffectCloudApplyBeforeEvent &source) {
                ServerNetworkHandler &owner = plugins.getOwner();
                std::string ids = "[";
                const auto append = [&ids](uint64_t id) {
                    if (ids.size() > 1)
                        ids += ',';
                    ids += std::to_string(id);
                };
                for (const uint64_t id: source.mPlayerRuntimeIds)
                    append(id);
                for (const int64_t id: source.mActorUniqueIds) {
                    if (ServerActor *actor = owner.getActor(id))
                        append(actor->getRuntimeId());
                }
                ids += ']';
                std::string effects = "[";
                for (const MobEffectInstance &instance: source.mEffects) {
                    if (effects.size() > 1)
                        effects += ',';
                    effects += '[' + std::to_string((int) instance.mId) + ',' + std::to_string(instance.mDuration) + ','
                               + std::to_string(instance.mAmplifier) + (instance.mParticles ? ",true" : ",false")
                               + (instance.mAmbient ? ",true]" : ",false]");
                }
                effects += ']';

                PluginEvent event;
                event.mType = FALCON_EVENT_AREA_EFFECT_CLOUD_APPLY;
                event.mCancellable = true;
                event.mCancelled = source.isCancelled();
                event.mEntity = &source.mCloud;
                event.mLevel = &source.mLevel;
                event.mCustomData = "{\"entities\":" + ids + ",\"effects\":" + effects + ",\"age\":"
                                    + std::to_string(source.mAge) + ",\"maxAge\":" + std::to_string(source.mMaxAge)
                                    + ",\"radius\":" + std::to_string(source.mRadius)
                                    + ",\"radiusChangeOnUse\":" + std::to_string(source.mRadiusOnUse)
                                    + ",\"radiusChangePerTick\":" + std::to_string(source.mRadiusPerTick)
                                    + ",\"reapplicationDelay\":" + std::to_string(source.mReapplicationDelay) + '}';
                const std::string original = event.mCustomData;
                plugins.dispatch(event);
                source.setCancelled(event.mCancelled);
                if (event.mCustomData != original) {
                    if (auto changed = decodeCloudEffects(event.mCustomData)) {
                        source.mEffects = std::move(*changed);
                        source.mEffectsChanged = true;
                    }
                }
            });

    plugins.hookBus(
            plugins.hasSubscribers(FALCON_EVENT_WORLD_INIT), after.mWorldInit,
            [&plugins](WorldOpenAfterEvent &source) { dispatchWorldOpen(plugins, FALCON_EVENT_WORLD_INIT, source); });

    plugins.hookBus(
            plugins.hasSubscribers(FALCON_EVENT_WORLD_LOAD), after.mWorldLoad,
            [&plugins](WorldOpenAfterEvent &source) { dispatchWorldOpen(plugins, FALCON_EVENT_WORLD_LOAD, source); });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_WORLD_UNLOAD), before.mWorldUnload,
                    [&plugins](WorldUnloadBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_WORLD_UNLOAD;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mWorldName = source.mName;
                        event.mLevel = &source.mWorld.getOverworld();
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_WORLD_SAVE), after.mWorldSave,
                    [&plugins](WorldSaveAfterEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_WORLD_SAVE;
                        event.mWorldName = source.mWorld.getName();
                        plugins.dispatch(event);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_WEATHER_CHANGE), before.mRainChange,
                    [&plugins](WeatherChangeBeforeEvent &source) {
                        dispatchWeather(plugins, FALCON_EVENT_WEATHER_CHANGE, source);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_THUNDER_CHANGE), before.mThunderChange,
                    [&plugins](WeatherChangeBeforeEvent &source) {
                        dispatchWeather(plugins, FALCON_EVENT_THUNDER_CHANGE, source);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_CHANGE_DIMENSION), before.mPlayerChangeDimension,
                    [&plugins](PlayerLevelChangeBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_CHANGE_DIMENSION;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mLevel = &source.mDestination;
                        event.mDimension = (uint32_t) Dimension::toId(source.mDestination.getDimensionType());
                        event.mPreviousDimension = (uint32_t) Dimension::toId(source.mPlayer.getDimension());
                        event.mFrom = source.mPlayer.getPosition();
                        event.mTo = source.mPosition;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                        if (event.mToChanged)
                            source.mPosition = event.mTo;
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_CHANGE_WORLD), before.mPlayerChangeWorld,
                    [&plugins](PlayerLevelChangeBeforeEvent &source) {
                        ServerNetworkHandler &owner = plugins.getOwner();
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_CHANGE_WORLD;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mLevel = &source.mDestination;
                        event.mWorldName = owner.getWorldOf(source.mDestination).getName();
                        event.mPreviousWorldName = owner.getWorldFor(source.mPlayer).getName();
                        event.mFrom = source.mPlayer.getPosition();
                        event.mTo = source.mPosition;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                        if (event.mToChanged)
                            source.mPosition = event.mTo;
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_DATA_SAVE), before.mPlayerDataSave,
                    [&plugins](PlayerDataSaveBeforeEvent &source) {
                        BinaryStream stream;
                        NbtIo::writeTag(stream, source.mData, NbtVariant::LittleEndian);
                        std::string payload = stream.getBuffer();
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_DATA_SAVE;
                        event.mPlayer = const_cast<ServerPlayer *>(&source.mPlayer);
                        event.mCustomName = source.mPlayer.getName();
                        event.mPacketData = &payload;
                        plugins.dispatch(event);
                        try {
                            ReadOnlyBinaryStream updated(payload);
                            Tag replacement = NbtIo::readTag(updated, NbtVariant::LittleEndian);
                            if (replacement.isCompound())
                                source.mData = std::move(replacement);
                        } catch (const std::exception &error) {
                            LOG_WARN(LogAreaID::Server, "Player save callback returned invalid data: %s", error.what());
                        }
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_CHUNK_LOAD), after.mChunkLoad,
                    [&plugins](ChunkAfterEvent &source) { dispatchChunk(plugins, FALCON_EVENT_CHUNK_LOAD, source); });

    plugins.hookBus(
            plugins.hasSubscribers(FALCON_EVENT_CHUNK_POPULATE), after.mChunkPopulate,
            [&plugins](ChunkAfterEvent &source) { dispatchChunk(plugins, FALCON_EVENT_CHUNK_POPULATE, source); });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_CHUNK_UNLOAD), after.mChunkUnload,
                    [&plugins](ChunkAfterEvent &source) { dispatchChunk(plugins, FALCON_EVENT_CHUNK_UNLOAD, source); });
}
