#include "World/Item/ArrowItem.h"

#include "World/Item/ItemClassRegistry.h"

FALCON_REGISTER_ITEM(ArrowItem, 50);

#include "World/Actor/ServerActor.h"
#include "World/Item/RangedWeaponHelpers.h"
#include "World/Level/Block/DispenseSource.h"
#include "Network/ServerNetworkHandler.h"

ArrowItem::ArrowItem(const Item &base) : Item(base) {}

bool ArrowItem::matches(const std::string &identifier) {
    return identifier == RangedWeaponHelpers::ARROW_IDENTIFIER;
}

// ArrowItem::dispense (sub_143F8FFC0, 26.60): the facing side 0.1 up; a tipped arrow carries aux - 1 as
// its potion (sub_143F8F7E0)
bool ArrowItem::dispense(const DispenseSource &source) const {
    ItemStack pickup = source.getItem();
    pickup.mCount = 1;

    Vector3f direction = source.getFacingVector();
    direction.y += 0.1f;
    ServerActor *arrow = source.mOwner.spawnDispensedProjectile(source.mLevel, RangedWeaponHelpers::ARROW_ACTOR,
                                                                source.mPosition, direction);
    if (arrow == nullptr)
        return false;

    ProjectileData &data = arrow->getProjectileData();
    data.mBaseDamage = RangedWeaponHelpers::ARROW_BASE_DAMAGE;
    data.mPotionId = pickup.mDamage - 1;
    data.mPickupItem = pickup;

    source.removeOne();
    source.playClick();
    source.broadcastShootEvent(DispenseSource::LEVEL_EVENT_SHOOT);
    return true;
}
