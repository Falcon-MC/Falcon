#pragma once

#include "Core/Event/GameEvents.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

template <typename T>
class EventSignal {
public:
    using Handler = std::function<void(T &)>;

    uint32_t subscribe(Handler handler) {
        const uint32_t id = mNextId++;
        mEntries.push_back(Entry{id, std::move(handler)});
        return id;
    }

    void unsubscribe(uint32_t id) {
        for (auto it = mEntries.begin(); it != mEntries.end(); ++it) {
            if (it->mId == id) {
                mEntries.erase(it);
                return;
            }
        }
    }

    void emit(T &event) {
        if (mEntries.empty())
            return;

        const std::vector<Entry> snapshot = mEntries;
        for (const Entry &entry: snapshot)
            entry.mHandler(event);
    }

    bool hasSubscribers() const {
        return !mEntries.empty();
    }

    size_t subscriberCount() const {
        return mEntries.size();
    }

    void clear() {
        mEntries.clear();
    }

private:
    struct Entry {
        uint32_t mId;
        Handler mHandler;
    };

    std::vector<Entry> mEntries;
    uint32_t mNextId = 1;
};

class BeforeEvents {
public:
    EventSignal<PlayerChatBeforeEvent> mChatSend;
    EventSignal<PlayerBreakBlockBeforeEvent> mPlayerBreakBlock;

    void clear() {
        mChatSend.clear();
        mPlayerBreakBlock.clear();
    }
};

class AfterEvents {
public:
    EventSignal<PlayerChatAfterEvent> mChatSend;
    EventSignal<PlayerBreakBlockAfterEvent> mPlayerBreakBlock;
    EventSignal<PlayerPlaceBlockAfterEvent> mPlayerPlaceBlock;
    EventSignal<PlayerSpawnAfterEvent> mPlayerSpawn;
    EventSignal<PlayerJoinAfterEvent> mPlayerJoin;
    EventSignal<PlayerLeaveAfterEvent> mPlayerLeave;
    EventSignal<PlayerGameModeChangeAfterEvent> mPlayerGameModeChange;
    EventSignal<EntityHurtAfterEvent> mEntityHurt;
    EventSignal<EntityDieAfterEvent> mEntityDie;
    EventSignal<ItemUseAfterEvent> mItemUse;
    EventSignal<PlayerHotbarSelectedSlotChangeAfterEvent> mPlayerHotbarSelectedSlotChange;
    EventSignal<EntityItemPickupAfterEvent> mEntityItemPickup;
    EventSignal<ItemStartUseAfterEvent> mItemStartUse;
    EventSignal<ItemCompleteUseAfterEvent> mItemCompleteUse;
    EventSignal<EffectAddAfterEvent> mEffectAdd;

    void clear() {
        mChatSend.clear();
        mPlayerBreakBlock.clear();
        mPlayerPlaceBlock.clear();
        mPlayerSpawn.clear();
        mPlayerJoin.clear();
        mPlayerLeave.clear();
        mPlayerGameModeChange.clear();
        mEntityHurt.clear();
        mEntityDie.clear();
        mItemUse.clear();
        mPlayerHotbarSelectedSlotChange.clear();
        mEntityItemPickup.clear();
        mItemStartUse.clear();
        mItemCompleteUse.clear();
        mEffectAdd.clear();
    }
};

class EventBus {
public:
    BeforeEvents &before() {
        return mBefore;
    }

    AfterEvents &after() {
        return mAfter;
    }

    void clear() {
        mBefore.clear();
        mAfter.clear();
    }

private:
    BeforeEvents mBefore;
    AfterEvents mAfter;
};
