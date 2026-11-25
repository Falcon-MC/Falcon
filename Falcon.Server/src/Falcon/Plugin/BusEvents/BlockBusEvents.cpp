#include "Falcon/Plugin/PluginBusEvents.h"

#include "Core/Event/EventBus.h"
#include "Core/Json/Json.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Falcon/Plugin/PluginWorldEventData.h"
#include "Protocol/Types/ItemStack.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace {
    Vector3f toVector3f(const Vector3i &position) {
        return Vector3f((float) position.x, (float) position.y, (float) position.z);
    }

    void hookBlockChange(PluginManager &plugins, FalconEventType type, EventSignal<BlockChangeBeforeEvent> &signal) {
        plugins.hookBus(plugins.hasSubscribers(type), signal, [&plugins, type](BlockChangeBeforeEvent &source) {
            PluginEvent event;
            event.mType = type;
            event.mCancellable = true;
            event.mCancelled = source.isCancelled();
            event.mLevel = &source.mLevel;
            event.mPlayer = source.mPlayer;
            event.mBlockPosition = source.mBlockPosition;
            event.mBlockName = source.mState.mName;
            event.mPreviousBlockName = source.mCurrent == nullptr ? std::string() : source.mCurrent->mName;
            event.mCustomData = PluginWorldEventData::encodeBlock(source.mState);
            if (source.mSource != nullptr)
                event.mFrom = toVector3f(*source.mSource);
            plugins.dispatch(event);
            source.setCancelled(event.mCancelled);
        });
    }

    // false when the list a plugin wrote is not valid: the structure then does not grow
    bool decodeStructure(Level &level, const std::string &data, BlockChangeSystem::StructureBlocks &blocks) {
        const auto changed = json::parse(data);
        if (changed == nullptr || !changed->isArray())
            return false;
        BlockChangeSystem::StructureBlocks replacements;
        replacements.reserve(changed->mArray.size());
        for (const auto &entry: changed->mArray) {
            if (!entry->isArray() || entry->mArray.size() != 4)
                return false;
            const auto &fields = entry->mArray;
            if (!fields[0]->isNumber() || !fields[1]->isNumber() || !fields[2]->isNumber())
                return false;
            const double x = fields[0]->number();
            const double y = fields[1]->number();
            const double z = fields[2]->number();
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || std::abs(x) > 30'000'000
                || std::abs(z) > 30'000'000 || y < level.getMinY() || y > level.getMaxY())
                return false;
            const Vector3i position((int32_t) x, (int32_t) y, (int32_t) z);
            BlockState state;
            if (!level.isChunkResident(position.x >> 4, position.z >> 4)
                || !PluginWorldEventData::decodeBlock(*fields[3], state))
                return false;
            replacements.emplace_back(position, std::move(state));
        }
        blocks = std::move(replacements);
        return true;
    }

    void hookFire(PluginManager &plugins, FalconEventType type, EventSignal<FireBeforeEvent> &signal) {
        plugins.hookBus(plugins.hasSubscribers(type), signal, [&plugins, type](FireBeforeEvent &source) {
            PluginEvent event;
            event.mType = type;
            event.mCancellable = true;
            event.mCancelled = source.isCancelled();
            event.mLevel = &source.mLevel;
            event.mBlockPosition = source.mBlockPosition;
            event.mBlockName = source.mBlock.mName;
            const Vector3i &fire = source.mFirePosition;
            event.mPosition = Vector3f((float) fire.x + 0.5f, (float) fire.y + 0.5f, (float) fire.z + 0.5f);
            plugins.dispatch(event);
            source.setCancelled(event.mCancelled);
        });
    }

    void hookPiston(PluginManager &plugins, FalconEventType type, EventSignal<PistonMoveBeforeEvent> &signal) {
        plugins.hookBus(plugins.hasSubscribers(type), signal, [&plugins, type](PistonMoveBeforeEvent &source) {
            std::vector<Vector3i> affected = source.mMoved;
            PluginEvent event;
            event.mType = type;
            event.mCancellable = true;
            event.mCancelled = source.isCancelled();
            event.mLevel = &source.mLevel;
            event.mBlockPosition = source.mBlockPosition;
            event.mBlockFace = (uint32_t) source.mFace;
            event.mBlocks = &affected;
            plugins.dispatch(event);
            source.setCancelled(event.mCancelled);
        });
    }
}

void PluginBusEvents::hookBlock(PluginManager &plugins, EventBus &bus) {
    BlockBeforeSignals &before = bus.before();

    hookBlockChange(plugins, FALCON_EVENT_BLOCK_GROW, before.mBlockGrow);
    hookBlockChange(plugins, FALCON_EVENT_BLOCK_SPREAD, before.mBlockSpread);
    hookBlockChange(plugins, FALCON_EVENT_BLOCK_FORM, before.mBlockForm);
    hookBlockChange(plugins, FALCON_EVENT_BLOCK_FADE, before.mBlockFade);
    hookBlockChange(plugins, FALCON_EVENT_LEAVES_DECAY, before.mLeavesDecay);

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_STRUCTURE_GROW), before.mStructureGrow,
                    [&plugins](StructureGrowBeforeEvent &source) {
                        std::string data = "[";
                        for (const auto &[position, state]: source.mBlocks) {
                            if (data.size() > 1)
                                data += ',';
                            data += '[' + std::to_string(position.x) + ',' + std::to_string(position.y) + ','
                                    + std::to_string(position.z) + ',' + PluginWorldEventData::encodeBlock(state) + ']';
                        }
                        data += ']';

                        PluginEvent event;
                        event.mType = FALCON_EVENT_STRUCTURE_GROW;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mLevel = &source.mLevel;
                        event.mPlayer = source.mPlayer;
                        event.mBlockPosition = source.mOrigin;
                        event.mCustomData = data;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                        if (event.mCancelled || event.mCustomData == data)
                            return;
                        if (!decodeStructure(source.mLevel, event.mCustomData, source.mBlocks))
                            source.cancel();
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_LIQUID_FLOW), before.mLiquidFlow,
                    [&plugins](LiquidFlowBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_LIQUID_FLOW;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mLevel = &source.mLevel;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mBlockName = source.mLiquid.mName;
                        event.mFrom = toVector3f(source.mSource);
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_REDSTONE_CHANGE), before.mRedstoneChange,
                    [&plugins](RedstoneChangeBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_REDSTONE_CHANGE;
                        event.mLevel = &source.mLevel;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mAmount = (double) source.mPower;
                        event.mPreviousAmount = (double) source.mPreviousPower;
                        plugins.dispatch(event);
                        source.mPower = std::clamp((int) event.mAmount, 0, RedstoneSystem::MAX_SIGNAL);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PRESSURE_PLATE_UPDATE), before.mPressurePlateUpdate,
                    [&plugins](PressurePlateUpdateBeforeEvent &source) {
                        std::string entities = "[";
                        for (const uint64_t id: source.mActivators) {
                            if (entities.size() > 1)
                                entities += ',';
                            entities += std::to_string(id);
                        }
                        entities += ']';

                        PluginEvent event;
                        event.mType = FALCON_EVENT_PRESSURE_PLATE_UPDATE;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mLevel = &source.mLevel;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mAmount = source.mStrength;
                        event.mPreviousAmount = source.mPreviousStrength;
                        const std::string original = "{\"block\":" + PluginWorldEventData::encodeBlock(source.mUpdated)
                                                     + ",\"entities\":" + entities + "}";
                        event.mCustomData = original;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                        if (event.mCancelled || event.mCustomData == original)
                            return;
                        const auto value = json::parse(event.mCustomData);
                        const auto *block = value != nullptr ? value->get("block") : nullptr;
                        // an unreadable state drops the update without a recheck
                        if (block == nullptr || !PluginWorldEventData::decodeBlock(*block, source.mUpdated)) {
                            source.mRecheck = false;
                            source.cancel();
                        }
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_BLOCK_PRE_EXPLODE), before.mBlockExplode,
                    [&plugins](BlockExplodeBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_BLOCK_PRE_EXPLODE;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mLevel = &source.mLevel;
                        event.mPlayer = source.mPlayer;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mAmount = source.mRadius;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled || !std::isfinite(event.mAmount) || event.mAmount < 0.0);
                        if (source.isCancelled())
                            return;
                        source.mRadius = event.mAmount;
                        if (const auto data = json::parse(event.mCustomData); data != nullptr && data->isObject()) {
                            if (const auto *blocks = data->get("breaks");
                                blocks != nullptr && blocks->mType == json::Value::Type::Boolean)
                                source.mBreaksBlocks = blocks->mBoolean;
                            if (const auto *fire = data->get("fire"); fire != nullptr && fire->isNumber())
                                source.mFireChance = std::clamp(fire->number(), 0.0, 1.0);
                        }
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_RESPAWN_ANCHOR_USE), before.mRespawnAnchorUse,
                    [&plugins](RespawnAnchorUseBeforeEvent &source) {
                        // the plugin api numbers the actions: 0 explodes, 1 sets the spawn
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_RESPAWN_ANCHOR_USE;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mLevel = &source.mLevel;
                        event.mPlayer = &source.mPlayer;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mAmount = source.mSetsSpawn ? 1.0 : 0.0;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                        if (event.mCancelled)
                            return;
                        if (event.mAmount == 1.0)
                            source.mSetsSpawn = true;
                        else if (event.mAmount == 0.0)
                            source.mSetsSpawn = false;
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_FARMLAND_HYDRATION_CHANGE), before.mFarmlandHydrationChange,
                    [&plugins](FarmlandHydrationChangeBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_FARMLAND_HYDRATION_CHANGE;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mLevel = &source.mLevel;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mPreviousAmount = source.mPreviousMoisture;
                        event.mAmount = source.mMoisture;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled || !std::isfinite(event.mAmount));
                        // farmland moisture runs from 0 to 7
                        if (!source.isCancelled())
                            source.mMoisture = (int32_t) std::clamp(event.mAmount, 0.0, 7.0);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_ENTITY_TRAMPLE_FARMLAND), before.mFarmlandTrample,
                    [&plugins](FarmlandTrampleBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_ENTITY_TRAMPLE_FARMLAND;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mLevel = &source.mLevel;
                        event.mEntity = &source.mEntity;
                        event.mBlockPosition = source.mBlockPosition;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    hookFire(plugins, FALCON_EVENT_BLOCK_BURN, before.mBlockBurn);
    hookFire(plugins, FALCON_EVENT_FIRE_SPREAD, before.mFireSpread);

    hookPiston(plugins, FALCON_EVENT_PISTON_EXTEND, before.mPistonExtend);
    hookPiston(plugins, FALCON_EVENT_PISTON_RETRACT, before.mPistonRetract);

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_BLOCK_UPDATE), before.mBlockUpdate,
                    [&plugins](BlockUpdateBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_BLOCK_UPDATE;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mLevel = &source.mLevel;
                        event.mBlockPosition = source.mBlockPosition;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PORTAL_CREATE), before.mPortalCreate,
                    [&plugins](PortalCreateBeforeEvent &source) {
                        // a copy: the portal fills the frame it found, whatever a plugin writes in the list
                        std::vector<Vector3i> shown = source.mBlocks;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PORTAL_CREATE;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mLevel = &source.mLevel;
                        event.mBlocks = &shown;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(
            plugins.hasSubscribers(FALCON_EVENT_BLOCK_TELEPORT), before.mBlockTeleport,
            [&plugins](BlockTeleportBeforeEvent &source) {
                PluginEvent event;
                event.mType = FALCON_EVENT_BLOCK_TELEPORT;
                event.mCancellable = true;
                event.mCancelled = source.isCancelled();
                event.mLevel = &source.mLevel;
                event.mBlockPosition = source.mBlockPosition;
                event.mTo = toVector3f(source.mDestination);
                plugins.dispatch(event);
                source.setCancelled(event.mCancelled);
                if (event.mCancelled || !event.mToChanged)
                    return;

                // a destination a plugin moved out of the world or into a block stops the teleport
                Level &level = source.mLevel;
                const Vector3f &to = event.mTo;
                if (!std::isfinite(to.x) || !std::isfinite(to.y) || !std::isfinite(to.z) || std::abs(to.x) > 30'000'000
                    || std::abs(to.z) > 30'000'000 || to.y < level.getMinY() || to.y > level.getMaxY()) {
                    source.cancel();
                    return;
                }
                const Vector3i destination((int32_t) std::floor(to.x), (int32_t) std::floor(to.y),
                                           (int32_t) std::floor(to.z));
                if (!level.isChunkResident(destination.x >> 4, destination.z >> 4)
                    || level.getBlockState(destination.x, destination.y, destination.z).mName != BlockNames::AIR) {
                    source.cancel();
                    return;
                }
                source.mDestination = destination;
            });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_BLOCK_DISPENSE), before.mBlockDispense,
                    [&plugins](BlockDispenseBeforeEvent &source) {
                        ItemStack fired = source.mItem;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_BLOCK_DISPENSE;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mLevel = &source.mLevel;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mBlockName = source.mBlock.mName;
                        event.mItem = &fired;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_BLOCK_DEATH), before.mBlockDeath,
                    [&plugins](BlockDeathBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_BLOCK_DEATH;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mLevel = &source.mLevel;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mBlockName = source.mNext.mName;
                        event.mPreviousBlockName = source.mCurrent.mName;
                        const std::string original = PluginWorldEventData::encodeBlock(source.mNext);
                        event.mCustomData = original;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                        if (event.mCancelled || event.mCustomData == original)
                            return;
                        BlockState altered;
                        if (!PluginWorldEventData::decodeBlock(event.mCustomData, altered)) {
                            source.cancel();
                            return;
                        }
                        source.mNext = std::move(altered);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_CHEST_PAIR), before.mChestPair,
                    [&plugins](ChestPairBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_CHEST_PAIR;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mLevel = &source.mLevel;
                        event.mBlockPosition = source.mLeft;
                        event.mTo = toVector3f(source.mRight);
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_BED_ENTER), before.mPlayerBedEnter,
                    [&plugins](PlayerBedEnterBeforeEvent &source) {
                        PluginEvent event;
                        event.mType = FALCON_EVENT_PLAYER_BED_ENTER;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mBlockPosition = source.mBedPosition;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_HOPPER_TRANSFER), before.mHopperTransfer,
                    [&plugins](HopperTransferBeforeEvent &source) {
                        ItemStack one = source.mSource;
                        one.mCount = 1;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_HOPPER_TRANSFER;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mLevel = &source.mLevel;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mItem = &one;
                        event.mState = source.mPushing;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_CAMPFIRE_COOK), before.mCampfireCook,
                    [&plugins](CampfireCookBeforeEvent &source) {
                        ItemStack input = source.mInput;
                        PluginEvent event;
                        event.mType = FALCON_EVENT_CAMPFIRE_COOK;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mLevel = &source.mLevel;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mSourceSlot = source.mSlot;
                        event.mItem = &input;
                        event.mResult = &source.mResult;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });
}
