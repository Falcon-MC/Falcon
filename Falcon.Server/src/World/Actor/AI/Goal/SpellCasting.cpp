#include "World/Actor/AI/Goal/SpellCasting.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Mob/MobActor.h"

namespace {
    const int32_t ACTOR_DATA_SPELL_CASTING_COLOR = 77;
}

void SpellCasting::setCasting(ServerNetworkHandler &owner, MobActor &mob, bool casting) {
    if (mob.getFlags().get(ActorFlag::Casting) == casting)
        return;
    mob.getFlags().set(ActorFlag::Casting, casting);
    owner.syncActorFlags(mob);
}

void SpellCasting::sendColor(ServerNetworkHandler &owner, MobActor &mob, int32_t color) {
    EntityDataEntry entry;
    entry.mId = ACTOR_DATA_SPELL_CASTING_COLOR;
    entry.mFormat = EntityDataFormat::Int;
    entry.mIntValue = color;
    EntityDataMap metadata;
    metadata.mEntries.push_back(entry);
    owner.sendActorMetadata(mob, metadata);
}
