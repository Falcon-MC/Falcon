#include "Block/Blocks/InfestedBlock.h"

#include "Actor/Mob/Hostile/SilverfishActor.h"
#include "Block/BlockClassRegistry.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/ActorEventPacket.h"

FALCON_REGISTER_BLOCK(InfestedBlock, 185);

namespace {
    struct InfestedPair {
        const char *mInfested;
        const char *mHost;
    };

    const InfestedPair INFESTED_PAIRS[] = {
            {"minecraft:infested_stone", "minecraft:stone"},
            {"minecraft:infested_cobblestone", "minecraft:cobblestone"},
            {"minecraft:infested_stone_bricks", "minecraft:stone_bricks"},
            {"minecraft:infested_mossy_stone_bricks", "minecraft:mossy_stone_bricks"},
            {"minecraft:infested_cracked_stone_bricks", "minecraft:cracked_stone_bricks"},
            {"minecraft:infested_chiseled_stone_bricks", "minecraft:chiseled_stone_bricks"},
            {"minecraft:infested_deepslate", "minecraft:deepslate"}
    };

    const EntityEventType SILVERFISH_SPAWN_ANIMATION = (EntityEventType) 27;
}

bool InfestedBlock::matches(const std::string &identifier) {
    for (const InfestedPair &pair: INFESTED_PAIRS) {
        if (identifier == pair.mInfested)
            return true;
    }
    return false;
}

bool InfestedBlock::infestedStateOf(const BlockState &host, BlockState &infested) {
    for (const InfestedPair &pair: INFESTED_PAIRS) {
        if (host.mName == pair.mHost) {
            infested = BlockState(pair.mInfested, host.mStates);
            return true;
        }
    }
    return false;
}

BlockState InfestedBlock::hostStateOf(const BlockState &state) const {
    for (const InfestedPair &pair: INFESTED_PAIRS) {
        if (state.mName == pair.mInfested)
            return BlockState(pair.mHost, state.mStates);
    }
    return state;
}

void InfestedBlock::release(ServerNetworkHandler &owner, Level &level, const Vector3i &position) const {
    level.setBlock(position, BlockState("minecraft:air"), true);

    const Vector3f center((float) position.x + 0.5f, (float) position.y, (float) position.z + 0.5f);
    ServerActor *silverfish = owner.spawnActor(level, SilverfishActor::IDENTIFIER, center);
    if (silverfish != nullptr)
        owner.broadcastActorEvent(*silverfish, SILVERFISH_SPAWN_ANIMATION);
}
