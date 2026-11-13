#include "World/Item/BrushItem.h"

#include "Network/ServerNetworkHandler.h"
#include "Protocol/BlockStateHasher.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/ItemClassRegistry.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/BrushableBlockActor.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Level.h"

#include <unordered_map>

FALCON_REGISTER_ITEM(BrushItem, 135);

namespace {
    // a stroke queues the block tick checking for a pause (sub_1433074C0)
    const int32_t RESET_CHECK_DELAY = 40;
    // the brushing sound repeats at most this often per player
    const int64_t SOUND_INTERVAL = 8;

    // the player's brushing component (hash 0xD0D19369): the tick after which the sound plays again
    std::unordered_map<uint64_t, int64_t> soundTicks;
}

BrushItem::BrushItem(const Item &base) : Item(base) {}

bool BrushItem::matches(const std::string &identifier) {
    return identifier == "minecraft:brush";
}

// BrushItem::useOn (sub_143F9B370, 26.60): any block takes the brush; only a brushable one counts the strokes
bool BrushItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                             const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const {
    (void) item;
    (void) clickPosition;
    Level &level = owner.getLevelFor(player);
    const BlockState clicked = level.getBlockState(blockPosition.x, blockPosition.y, blockPosition.z);
    if (clicked.mName == BlockNames::AIR)
        return false;

    const int64_t tick = owner.getCurrentTick();
    BrushableBlockActor *actor = level.getBlockActors().find<BrushableBlockActor>(blockPosition);
    bool rollLoot = false;
    if (actor != nullptr && face >= 0 && face < 6 && actor->stroke(tick, (uint8_t) face, rollLoot)) {
        if (rollLoot)
            actor->unpackLoot(owner);
        level.scheduleUpdate(blockPosition, RESET_CHECK_DELAY);
        if (actor->getBrushCount() >= BrushableBlockActor::BRUSHES_TO_COMPLETE) {
            actor->complete(owner, level);
            owner.damagePlayerHeldItem(player, 1);
            if (player.getInventory().getItemInHand().isAir())
                return true;
        } else {
            actor->sync(owner, level);
        }
    }

    int64_t &next = soundTicks[player.getRuntimeId()];
    if (tick > next) {
        // the default state of the clicked block, not its current one
        const Tag *defaults = BlockPaletteRegistry::getInstance().getDefaultStates(clicked.mName);
        const BlockState sound = defaults != nullptr ? BlockState(clicked.mName, *defaults) : clicked;
        owner.playBlockSound(level, "brush",
                             Vector3f((float) blockPosition.x + 0.5f, (float) blockPosition.y + 0.5f,
                                      (float) blockPosition.z + 0.5f),
                             (int32_t) BlockStateHasher::hash(sound.mName, sound.mStates));
        next = tick + SOUND_INTERVAL;
    }
    return true;
}
