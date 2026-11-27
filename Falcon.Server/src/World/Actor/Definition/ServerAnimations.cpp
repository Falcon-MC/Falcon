#include "World/Actor/Definition/ServerAnimations.h"

#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "World/Actor/Definition/EntityEvents.h"
#include "World/Actor/Definition/Molang.h"
#include "World/Actor/ServerActor.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <map>
#include <unordered_map>

namespace {
    constexpr double TICK_SECONDS = 1.0 / 20.0;
    constexpr size_t MAX_DEPTH = 32;
    constexpr size_t MAX_ANIMATIONS_PER_ACTOR_TICK = 1024;

    using Actions = std::vector<std::string>;

    const json::Value &member(const json::Value &object, const char *name) {
        static const json::Value EMPTY;
        const auto *value = object.get(name);
        return value == nullptr ? EMPTY : *value;
    }

    Actions readActions(const json::Value &value) {
        if (value.isString())
            return {value.mString};
        Actions result;
        for (const auto &action: value.mArray) {
            if (action->isString())
                result.push_back(action->mString);
        }
        return result;
    }

    std::string expression(const json::Value &value) {
        if (value.isString())
            return value.mString;
        if (value.isNumber())
            return std::to_string(value.mNumber);
        if (value.mType == json::Value::Type::Boolean)
            return value.mBoolean ? "1" : "0";
        return {};
    }

    struct Link {
        std::string mAlias;
        std::string mCondition;
    };

    std::vector<Link> readLinks(const json::Value &value) {
        std::vector<Link> result;
        for (const auto &link: value.mArray) {
            if (link->isString())
                result.push_back({link->mString, {}});
            else if (link->mKeys.size() == 1)
                result.push_back({link->mKeys[0], expression(*link->get(link->mKeys[0]))});
        }
        return result;
    }

    struct Timeline {
        std::map<double, Actions> mFrames;
        double mLength = 0.0;
        std::string mStartDelay;
        std::string mLoopDelay;
        std::string mTimeUpdate;
        bool mLoop = false;
    };

    struct State {
        Actions mEntry;
        Actions mExit;
        std::vector<Link> mAnimations;
        std::vector<Link> mTransitions;
    };

    struct Controller {
        std::string mInitialState;
        std::map<std::string, State, std::less<>> mStates;
    };

    struct Asset {
        const Timeline *mTimeline = nullptr;
        const Controller *mController = nullptr;
    };

    struct Entity {
        std::map<std::string, std::string, std::less<>> mAliases;
        std::map<std::string, Asset, std::less<>> mAssets;
        std::vector<Link> mAnimations;
        Actions mInitialize;
        Actions mPreAnimation;
    };

    struct Track {
        const State *mState = nullptr;
        double mTime = 0.0;
        double mWait = 0.0;
        bool mStarted = false;
        bool mFinished = false;
        bool mWaitingForLoop = false;
        bool mHasFrame = false;
        bool mActive = false;
        std::vector<Track> mChildren;
    };

    struct ActorTracks {
        std::vector<Track> mTracks;
        std::unordered_map<std::string, MolangValue> mVariables;
    };

    std::map<std::string, Timeline, std::less<>> timelines;
    std::map<std::string, Controller, std::less<>> controllers;
    std::map<std::string, Entity, std::less<>> entities;
    std::unordered_map<uint64_t, ActorTracks> active;

    Asset findAsset(const std::string &id) {
        if (const auto controller = controllers.find(id); controller != controllers.end())
            return {nullptr, &controller->second};
        if (const auto timeline = timelines.find(id); timeline != timelines.end())
            return {&timeline->second, nullptr};
        return {};
    }

    void readTimelines(const json::Value &document) {
        const auto &definitions = member(document, "animations");
        for (const auto &id: definitions.mKeys) {
            const auto &definition = *definitions.get(id);
            Timeline timeline;
            const auto &frames = member(definition, "timeline");
            for (const auto &time: frames.mKeys) {
                char *end = nullptr;
                const double seconds = std::strtod(time.c_str(), &end);
                if (end != time.c_str() && *end == '\0' && std::isfinite(seconds) && seconds >= 0.0)
                    timeline.mFrames[seconds] = readActions(*frames.get(time));
            }
            const double lastFrame = timeline.mFrames.empty() ? 0.0 : timeline.mFrames.rbegin()->first;
            timeline.mLength = std::max(lastFrame, member(definition, "animation_length").number(lastFrame));
            timeline.mLoop = member(definition, "loop").boolean();
            timeline.mStartDelay = expression(member(definition, "start_delay"));
            timeline.mLoopDelay = expression(member(definition, "loop_delay"));
            timeline.mTimeUpdate = expression(member(definition, "anim_time_update"));
            timelines[id] = std::move(timeline);
        }
    }

    void readControllers(const json::Value &document) {
        const auto &definitions = member(document, "animation_controllers");
        for (const auto &id: definitions.mKeys) {
            const auto &definition = *definitions.get(id);
            Controller controller;
            controller.mInitialState = member(definition, "initial_state").string("default");
            const auto &states = member(definition, "states");
            for (const auto &name: states.mKeys) {
                const auto &state = *states.get(name);
                controller.mStates.emplace(name, State{readActions(member(state, "on_entry")),
                                                        readActions(member(state, "on_exit")),
                                                        readLinks(member(state, "animations")),
                                                        readLinks(member(state, "transitions"))});
            }
            if (controller.mStates.contains(controller.mInitialState))
                controllers[id] = std::move(controller);
        }
    }

    void readEntity(const json::Value &document) {
        const auto &definition = member(document, "minecraft:entity");
        const auto &description = member(definition, "description");
        const std::string id = member(description, "identifier").string();
        if (id.empty())
            return;
        const auto &scripts = member(description, "scripts");
        Entity entity;
        entity.mAnimations = readLinks(member(scripts, "animate"));
        entity.mInitialize = readActions(member(scripts, "initialize"));
        entity.mPreAnimation = readActions(member(scripts, "pre_animation"));
        const auto &aliases = member(description, "animations");
        for (const auto &alias: aliases.mKeys) {
            if (aliases.get(alias)->isString())
                entity.mAliases.emplace(alias, aliases.get(alias)->mString);
        }
        if (!entity.mAnimations.empty() || !entity.mInitialize.empty() || !entity.mPreAnimation.empty())
            entities[id] = std::move(entity);
        else
            entities.erase(id);
    }

    class Playback {
    public:
        Playback(ServerNetworkHandler &owner, ServerActor &actor, const Entity &entity, ActorTracks &tracks)
                : mOwner(owner), mActor(actor), mEntity(entity) {
            mContext.mVariables = &tracks.mVariables;
        }

        void run(const Actions &actions) {
            for (const auto &action: actions) {
                if (mActions >= MAX_ANIMATIONS_PER_ACTOR_TICK)
                    break;
                ++mActions;
                if (action.starts_with("@s "))
                    EntityEvents::fire(mOwner, mActor, action.substr(3));
                else if (action.starts_with('/'))
                    mOwner.queueActorCommand(mActor, action, true);
                else if (!action.empty() && action.size() <= 1024)
                    Molang::evaluate(action, mActor, mContext);
            }
        }

        void play(const std::vector<Link> &links, std::vector<Track> &tracks, size_t depth) {
            if (depth > MAX_DEPTH)
                return;
            const size_t count = std::min(links.size(), MAX_ANIMATIONS_PER_ACTOR_TICK);
            for (size_t i = 0; i < count && mTracks < MAX_ANIMATIONS_PER_ACTOR_TICK; ++i) {
                const Link &link = links[i];
                if (!enabled(link.mCondition)) {
                    if (i < tracks.size() && tracks[i].mActive)
                        tracks[i] = {};
                    continue;
                }
                ++mTracks;
                if (i >= tracks.size())
                    tracks.resize(i + 1);
                Track &track = tracks[i];
                const auto known = mEntity.mAssets.find(link.mAlias);
                const Asset asset = known == mEntity.mAssets.end() ? findAsset(link.mAlias) : known->second;
                track.mActive = asset.mController != nullptr || asset.mTimeline != nullptr;
                if (asset.mController != nullptr)
                    playController(*asset.mController, track, depth);
                else if (asset.mTimeline != nullptr)
                    playTimeline(*asset.mTimeline, track);
            }
        }

    private:
        bool enabled(const std::string &condition) {
            return condition.empty() || (condition.size() <= 1024 && Molang::evaluate(condition, mActor, mContext).isTrue());
        }

        double timeOf(const std::string &value, double fallback = 0.0) {
            if (value.empty() || value.size() > 1024)
                return fallback;
            const double time = Molang::evaluate(value, mActor, mContext).mNumber;
            return std::isfinite(time) ? std::max(0.0, time) : fallback;
        }

        void playController(const Controller &controller, Track &track, size_t depth) {
            const MolangContext parent = mContext;
            const auto enter = [&](const State &state) {
                track.mState = &state;
                track.mTime = 0.0;
                track.mChildren.clear();
                mContext.mAnimationTime = 0.0;
                mContext.mStateTime = 0.0;
                mContext.mAnyAnimationFinished = false;
                mContext.mAllAnimationsFinished = false;
                run(state.mEntry);
            };
            if (track.mState == nullptr)
                enter(controller.mStates.at(controller.mInitialState));

            mContext.mStateTime = track.mTime;
            play(track.mState->mAnimations, track.mChildren, depth + 1);
            mContext.mAnyAnimationFinished = std::any_of(track.mChildren.begin(), track.mChildren.end(),
                    [](const Track &child) { return child.mActive && child.mFinished; });
            mContext.mAllAnimationsFinished = mContext.mAnyAnimationFinished
                    && std::none_of(track.mChildren.begin(), track.mChildren.end(),
                                    [](const Track &child) { return child.mActive && !child.mFinished; });
            track.mTime += TICK_SECONDS;
            for (const Link &transition: track.mState->mTransitions) {
                const auto state = controller.mStates.find(transition.mAlias);
                if (state == controller.mStates.end() || !enabled(transition.mCondition))
                    continue;
                run(track.mState->mExit);
                enter(state->second);
                break;
            }
            mContext = parent;
        }

        void playTimeline(const Timeline &timeline, Track &track) {
            if (track.mFinished)
                return;
            const MolangContext parent = mContext;
            if (!track.mStarted && !track.mWaitingForLoop) {
                track.mWait = timeOf(timeline.mStartDelay);
                track.mStarted = true;
            }
            if (track.mWait > 1e-9) {
                track.mWait -= TICK_SECONDS;
                return;
            }
            if (track.mWaitingForLoop) {
                track.mTime = 0.0;
                track.mHasFrame = false;
                track.mWaitingForLoop = false;
                track.mStarted = true;
            }

            const double old = track.mTime;
            mContext.mAnimationTime = old;
            const double next = timeOf(timeline.mTimeUpdate, old + TICK_SECONDS);
            const auto first = track.mHasFrame ? timeline.mFrames.upper_bound(old + 1e-9) : timeline.mFrames.begin();
            track.mHasFrame = true;
            for (auto frame = first; frame != timeline.mFrames.end() && frame->first <= next + 1e-9; ++frame) {
                mContext.mAnimationTime = frame->first;
                run(frame->second);
            }
            track.mTime = next;
            if (next + 1e-9 >= timeline.mLength) {
                if (timeline.mLoop && timeline.mLength > 0.0) {
                    track.mWaitingForLoop = true;
                    track.mWait = timeOf(timeline.mLoopDelay);
                } else {
                    track.mFinished = true;
                }
            }
            mContext = parent;
        }

        ServerNetworkHandler &mOwner;
        ServerActor &mActor;
        const Entity &mEntity;
        MolangContext mContext;
        size_t mTracks = 0;
        size_t mActions = 0;
    };
}

void ServerAnimations::loadPacks(const std::vector<BehaviorPack> &packs) {
    active.clear();
    entities.clear();
    controllers.clear();
    timelines.clear();
    for (const auto &pack: packs) {
        pack.visitJson("animations", readTimelines);
        pack.visitJson("animation_controllers", readControllers);
        pack.visitJson("entities", readEntity);
    }
    for (auto &[name, entity]: entities) {
        for (const auto &[alias, id]: entity.mAliases)
            entity.mAssets.emplace(alias, findAsset(id));
        entity.mAliases.clear();
    }
}

void ServerAnimations::tick(ServerNetworkHandler &owner, ServerActor &actor) {
    if (entities.empty())
        return;
    const auto definition = entities.find(actor.getIdentifier());
    if (definition == entities.end())
        return;
    const auto [tracks, fresh] = active.try_emplace(actor.getRuntimeId());
    Playback playback(owner, actor, definition->second, tracks->second);
    if (fresh)
        playback.run(definition->second.mInitialize);
    playback.run(definition->second.mPreAnimation);
    playback.play(definition->second.mAnimations, tracks->second.mTracks, 0);
}

void ServerAnimations::forget(uint64_t runtimeId) {
    active.erase(runtimeId);
}
