#include "Falcon/Plugin/PluginBusEvents.h"

#include "Core/Event/EventBus.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Protocol/Types/ItemStack.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Level.h"

namespace {
    const char *causeName(TeleportCause cause) {
        switch (cause) {
            case TeleportCause::Command:
                return "command";
            case TeleportCause::ChorusFruit:
                return "chorus_fruit";
            case TeleportCause::EnderPearl:
                return "ender_pearl";
            case TeleportCause::EndGateway:
                return "end_gateway";
            case TeleportCause::FollowOwner:
                return "follow_owner";
            case TeleportCause::Plugin:
                return "plugin";
        }
        return "";
    }
}

void PluginBusEvents::hookTeleport(PluginManager &plugins, EventBus &bus) {
    TeleportBeforeSignals &before = bus.before();

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_TELEPORT)
                            || plugins.hasSubscribers(FALCON_EVENT_ENTITY_TELEPORT),
                    before.mActorTeleport, [&plugins](ActorTeleportBeforeEvent &source) {
                        ServerPlayer *player = dynamic_cast<ServerPlayer *>(&source.mActor);
                        const FalconEventType type = player != nullptr ? FALCON_EVENT_PLAYER_TELEPORT
                                                                       : FALCON_EVENT_ENTITY_TELEPORT;
                        if (!plugins.hasSubscribers(type))
                            return;

                        PluginEvent event;
                        event.mType = type;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = player;
                        event.mEntity = &source.mActor;
                        event.mLevel = source.mDestination;
                        event.mFrom = source.mActor.getPosition();
                        event.mTo = source.mTo;
                        event.mCause = causeName(source.mCause);
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                        if (source.isCancelled())
                            return;
                        if (event.mToChanged)
                            source.mTo = event.mTo;
                        source.mDestination = event.mLevel;
                    });

    plugins.hookBus(plugins.hasSubscribers(FALCON_EVENT_PLAYER_BUCKET_FILL)
                            || plugins.hasSubscribers(FALCON_EVENT_PLAYER_BUCKET_EMPTY),
                    before.mPlayerBucketUse, [&plugins](PlayerBucketUseBeforeEvent &source) {
                        const FalconEventType type = source.mFill ? FALCON_EVENT_PLAYER_BUCKET_FILL
                                                                  : FALCON_EVENT_PLAYER_BUCKET_EMPTY;
                        if (!plugins.hasSubscribers(type))
                            return;

                        ItemStack bucket = source.mBucket;
                        PluginEvent event;
                        event.mType = type;
                        event.mCancellable = true;
                        event.mCancelled = source.isCancelled();
                        event.mPlayer = &source.mPlayer;
                        event.mLevel = &source.mLevel;
                        event.mItem = &bucket;
                        event.mBlockPosition = source.mBlockPosition;
                        event.mBlockName = source.mLevel
                                                   .getBlockState(source.mBlockPosition.x, source.mBlockPosition.y,
                                                                  source.mBlockPosition.z)
                                                   .mName;
                        plugins.dispatch(event);
                        source.setCancelled(event.mCancelled);
                    });
}
