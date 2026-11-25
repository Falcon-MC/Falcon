#include "World/Item/GlassBottleItem.h"

#include "World/Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM(GlassBottleItem, 100);

#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/BucketItem.h"
#include "World/Level/Block/Actor/HopperBlockActor.h"
#include "World/Level/Block/BeehiveBlock.h"
#include "World/Level/Block/DispenseSource.h"
#include "World/Level/Block/DispenserBlock.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>

namespace {
    const char *const WATER_BLOCK = "minecraft:water";
    const char *const WATER_BOTTLE = "minecraft:potion";
    const char *const FILL_SOUND = "bottle.fill";
    const char *const HONEY_BOTTLE = "minecraft:honey_bottle";
}

GlassBottleItem::GlassBottleItem(const Item &base) : Item(base) {}

bool GlassBottleItem::matches(const std::string &identifier) {
    return identifier == IDENTIFIER;
}

bool GlassBottleItem::onUseOnBlock(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item,
                                   const Vector3i &blockPosition, int32_t face, const Vector3f &clickPosition) const {
    (void) face;
    (void) clickPosition;

    Level &level = owner.getLevelFor(player);
    if (level.getBlockState(blockPosition.x, blockPosition.y, blockPosition.z).mName != WATER_BLOCK)
        return false;

    const Vector3f center((float) blockPosition.x + 0.5f, (float) blockPosition.y + 0.5f,
                          (float) blockPosition.z + 0.5f);
    BucketItem::sendArmSwing(owner, player, blockPosition);
    owner.playLevelSound(level, FILL_SOUND, center);
    return BucketItem::applyResult(owner, player, item, WATER_BOTTLE);
}

bool GlassBottleItem::dispense(const DispenseSource &source) const {
    const Vector3i block((int32_t) std::floor(source.mPosition.x), (int32_t) std::floor(source.mPosition.y),
                         (int32_t) std::floor(source.mPosition.z));
    if (!BeehiveBlock::isHarvestable(source.mLevel.getBlockState(block.x, block.y, block.z)))
        return false;

    // only the dispenser's click: no fill sound, and the bees stay calm
    source.playClick();
    source.removeOne();
    const ItemStack honey = source.mOwner.createItemStack(HONEY_BOTTLE, 1);
    if (HopperBlockActor::addOne(source.mContainer, honey))
        source.mOwner.refreshContainerViewers(source.mBlockPosition);
    else
        DispenserBlock::ejectItem(source.mOwner, source.mLevel, source.mBlockPosition, source.mFace, honey);
    BeehiveBlock::setHoneyLevel(source.mLevel, block, 0);
    source.mLevel.postGameEvent(GameEvents::FLUID_PICKUP, source.mPosition);
    return true;
}
