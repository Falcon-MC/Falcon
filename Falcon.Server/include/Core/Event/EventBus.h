#pragma once

#include "Core/Event/EventSignal.h"
#include "Core/Event/GameEvents.h"
#include "Core/Event/BrewingEvents.h"
#include "Core/Event/ActorEvents.h"
#include "Core/Event/LevelEvents.h"
#include "Core/Event/BlockEvents.h"
#include "Core/Event/InventoryEvents.h"
#include "Core/Event/ServerEvents.h"
#include "Core/Event/PlayerEvents.h"
#include "Core/Event/TeleportEvents.h"

class BeforeEvents : public BrewingBeforeSignals,
                     public ActorBeforeSignals,
                     public LevelBeforeSignals,
                     public BlockBeforeSignals,
                     public InventoryBeforeSignals,
                     public ServerBeforeSignals,
                     public PlayerBeforeSignals,
                     public TeleportBeforeSignals {
public:
    EventSignal<PlayerChatBeforeEvent> mChatSend;
    EventSignal<PlayerBreakBlockBeforeEvent> mPlayerBreakBlock;
    EventSignal<PlayerJoinMessageBeforeEvent> mPlayerJoinMessage;
    EventSignal<PlayerLeaveMessageBeforeEvent> mPlayerLeaveMessage;

    void clear() {
        mChatSend.clear();
        mPlayerBreakBlock.clear();
        mPlayerJoinMessage.clear();
        mPlayerLeaveMessage.clear();
        clearBrewingBeforeSignals();
        clearActorBeforeSignals();
        clearLevelBeforeSignals();
        clearBlockBeforeSignals();
        clearInventoryBeforeSignals();
        clearServerBeforeSignals();
        clearPlayerBeforeSignals();
        clearTeleportBeforeSignals();
    }
};

class AfterEvents : public BrewingAfterSignals,
                    public ActorAfterSignals,
                    public LevelAfterSignals,
                    public BlockAfterSignals,
                    public InventoryAfterSignals,
                    public ServerAfterSignals,
                    public PlayerAfterSignals,
                    public TeleportAfterSignals {
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
        clearBrewingAfterSignals();
        clearActorAfterSignals();
        clearLevelAfterSignals();
        clearBlockAfterSignals();
        clearInventoryAfterSignals();
        clearServerAfterSignals();
        clearPlayerAfterSignals();
        clearTeleportAfterSignals();
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
