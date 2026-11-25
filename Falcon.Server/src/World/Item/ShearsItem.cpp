#include "World/Item/ShearsItem.h"

#include "World/Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM(ShearsItem, 130);

#include "World/Actor/ActorFlags.h"
#include "World/Actor/Mob/Passive/SheepActor.h"
#include "World/Item/ItemDurability.h"
#include "World/Inventory/Container.h"
#include "World/Level/Block/BeehiveBlock.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/DispenseSource.h"
#include "World/Level/Block/CobwebBlock.h"
#include "World/Level/Block/PumpkinBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/VineBlock.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "World/Level/Vibration/GameEvent.h"

#include <cmath>

namespace {
    const float WEB_AND_LEAVES_SPEED = 15.0f;
    const float WOOL_SPEED = 5.0f;
    const float VINE_SPEED = 2.0f;
    const char *const SHEAR_SOUND = "shear";
    // covers a mob whose box overlaps the faced block from a neighbouring one
    const float DISPENSE_SEARCH_RADIUS = 2.0f;

    bool overlapsBlock(const MobActor &actor, const Vector3i &block) {
        const Vector3f position = actor.getPosition();
        const ActorSize size = actor.getSize();
        const float halfWidth = size.mWidth * 0.5f;
        return position.x + halfWidth > (float) block.x && position.x - halfWidth < (float) block.x + 1.0f
               && position.z + halfWidth > (float) block.z && position.z - halfWidth < (float) block.z + 1.0f
               && position.y + size.mHeight > (float) block.y && position.y < (float) block.y + 1.0f;
    }

    void wearFromDispenser(const DispenseSource &source) {
        ItemStack shears = source.getItem();
        ItemDurability::apply(nullptr, shears, 1);
        source.mContainer.setContainerItem(source.mSlot, std::move(shears));
        source.mOwner.refreshContainerViewers(source.mBlockPosition);
        source.playClick();
        source.mLevel.postGameEvent(GameEvents::SHEAR, source.mPosition);
    }
}

ShearsItem::ShearsItem(const Item &base) : Item(base) {}

bool ShearsItem::matches(const std::string &identifier) {
    return identifier == "minecraft:shears";
}

bool ShearsItem::getDestroySpeed(const std::string &blockIdentifier, float &speed) const {
    const Block *block = VanillaBlocks::fromIdentifier(blockIdentifier);
    if (dynamic_cast<const CobwebBlock *>(block) != nullptr || (block != nullptr && block->isLeaves())) {
        speed = WEB_AND_LEAVES_SPEED;
        return true;
    }

    // besides wool, the data table only gives shears to instantly broken plants
    const BlockData *data = BlockDataTable::find(blockIdentifier.c_str());
    if (data != nullptr && data->mToolType == BlockToolType::Shears) {
        speed = WOOL_SPEED;
        return true;
    }

    // glow lichen has no block class; the client lists it next to vines
    if (dynamic_cast<const VineBlock *>(block) != nullptr || blockIdentifier == "minecraft:glow_lichen") {
        speed = VINE_SPEED;
        return true;
    }

    (void) speed;
    return false;
}

bool ShearsItem::canDestroySpecial(const std::string &blockIdentifier) const {
    return dynamic_cast<const CobwebBlock *>(VanillaBlocks::fromIdentifier(blockIdentifier)) != nullptr;
}

bool ShearsItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                              const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const {
    (void) item;
    (void) clickPosition;
    const BlockState &state =
            owner.getLevelFor(player).getBlockState(blockPosition.x, blockPosition.y, blockPosition.z);
    const PumpkinBlock *pumpkin = dynamic_cast<const PumpkinBlock *>(VanillaBlocks::fromState(state));
    if (pumpkin == nullptr)
        return false;
    pumpkin->carve(owner, player, blockPosition, face);
    return true;
}

bool ShearsItem::dispense(const DispenseSource &source) const {
    const Vector3i block((int32_t) std::floor(source.mPosition.x), (int32_t) std::floor(source.mPosition.y),
                         (int32_t) std::floor(source.mPosition.z));
    // a full hive comes before any mob; its bees stay calm, unlike when a player shears it
    if (BeehiveBlock::isHarvestable(source.mLevel.getBlockState(block.x, block.y, block.z))) {
        BeehiveBlock::dropHoneycombs(source.mOwner, source.mLevel, block);
        BeehiveBlock::setHoneyLevel(source.mLevel, block, 0);
        source.mOwner.playLevelSound(source.mLevel, LevelSoundEvent::BEEHIVE_SHEAR, source.mPosition);
        wearFromDispenser(source);
        return true;
    }

    const Vector3f center((float) block.x + 0.5f, (float) block.y + 0.5f, (float) block.z + 0.5f);
    SheepActor *sheep = nullptr;
    source.mOwner.forEachActorNear(center, DISPENSE_SEARCH_RADIUS, [&](ServerActor &actor) {
        auto *candidate = dynamic_cast<SheepActor *>(&actor);
        // the game reads the flags; their components are read too: the flags sync on the mob's next tick
        if (sheep != nullptr || candidate == nullptr || !candidate->isAlive() || !candidate->isIn(source.mLevel)
            || candidate->getFlags().get(ActorFlag::Sheared) || candidate->getFlags().get(ActorFlag::Baby)
            || candidate->getComponent("minecraft:is_sheared") != nullptr
            || candidate->getComponent("minecraft:is_baby") != nullptr || !overlapsBlock(*candidate, block))
            return;
        sheep = candidate;
    });
    // the game keeps the shears in the dispenser even with nothing to shear, unworn and silent
    if (sheep == nullptr)
        return true;

    sheep->shearFromDispenser(source.mOwner);
    source.mOwner.playLevelSound(source.mLevel, SHEAR_SOUND, source.mPosition);
    wearFromDispenser(source);
    return true;
}
