#include "World/Actor/Mob/Component/MobLeash.h"

#include "Core/Json/Json.h"
#include "Core/NBT/Tag.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Definition/EntityEvents.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Storage/Loot/LootItems.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/EntityDataMap.h"

#include <cmath>
#include <memory>

namespace {
    const char *const LEASHABLE_COMPONENT = "minecraft:leashable";
    const char *const TAG_LEASHER = "LeasherID";
    const char *const LEAD_ITEM = "minecraft:lead";

    const int32_t LEASH_HOLDER_DATA_ID = 37;

    // Defaults of the leashable preset in 1.26.52.
    const float DEFAULT_SOFT_DISTANCE = 4.0f;
    const float DEFAULT_HARD_DISTANCE = 7.0f;
    const float DEFAULT_MAX_DISTANCE = 12.0f;

    const float FOLLOW_SPEED = 1.0f;
    const float BOUNCY_PULL = 0.2f;
    const float SPRING_HORIZONTAL = 0.8f;
    const float SPRING_VERTICAL = 0.2f;
    const float SPRING_TOWARDS_ONLY = 0.3f;
    const float VELOCITY_MATCH = 0.11f;
    const float MIN_DIRECTION_LENGTH = 0.0001f;
    const float DEGREES_TO_RADIANS = 0.017453292f;

    struct AttachPoint {
        float x;
        float y;
        float z;
    };

    // Local attachment points scaled by the half extents: the mob's front for the dampened spring, its
    // four lower corners for the quad spring.
    const AttachPoint DAMPENED_MOB_POINTS[] = {{0.0f, 0.0f, 1.0f}};
    const AttachPoint DAMPENED_HOLDER_POINTS[] = {{0.0f, 0.0f, 0.0f}};
    const AttachPoint QUAD_POINTS[] = {{-1.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, -1.0f}, {1.0f, 0.0f, -1.0f},
                                       {1.0f, 0.0f, 1.0f}};

    float numberOr(const json::Value *object, const char *key, float fallback) {
        const json::Value *value = object == nullptr ? nullptr : object->get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    struct Box {
        Vector3f mCenter;
        Vector3f mHalf;
        float mYaw;
    };

    Box boxOf(const Actor &actor, float width, float height, float yawAdjustment) {
        const Vector3f &position = actor.getPosition();
        return Box{Vector3f(position.x, position.y + height * 0.5f, position.z),
                   Vector3f(width * 0.5f, height * 0.5f, width * 0.5f), actor.getRotation().y - yawAdjustment};
    }

    Box boxOf(const Actor &actor, float yawAdjustment) {
        if (actor.isPlayer())
            return boxOf(actor, MovementPhysics::PLAYER_WIDTH, MovementPhysics::PLAYER_HEIGHT, yawAdjustment);

        const ActorSize size = static_cast<const ServerActor &>(actor).getSize();
        return boxOf(actor, size.mWidth, size.mHeight, yawAdjustment);
    }

    Vector3f worldPoint(const Box &box, const AttachPoint &point) {
        const float angle = box.mYaw * -DEGREES_TO_RADIANS;
        const float sine = std::sin(angle);
        const float cosine = std::cos(angle);
        const float localX = point.x * box.mHalf.x;
        const float localZ = point.z * box.mHalf.z;
        return Vector3f(box.mCenter.x + cosine * localX + sine * localZ, box.mCenter.y + point.y * box.mHalf.y,
                        box.mCenter.z + cosine * localZ - sine * localX);
    }

    Actor *findHolder(ServerNetworkHandler &owner, int64_t uniqueId) {
        for (auto &entry: owner.getPlayers()) {
            ServerPlayer &player = entry.second;
            if (player.getUniqueId() == uniqueId)
                return player.isSpawned() && !player.isDead() ? &player : nullptr;
        }
        return nullptr;
    }
}

void MobLeash::attach(ServerNetworkHandler &owner, MobActor &mob, int64_t holderUniqueId) {
    mHolderId = holderUniqueId;
    _sync(owner, mob);

    const json::Value *leashable = mob.getComponent(LEASHABLE_COMPONENT);
    if (leashable != nullptr)
        EntityEvents::fireTrigger(owner, mob, leashable->get("on_leash"), findHolder(owner, holderUniqueId));
}

void MobLeash::detach(ServerNetworkHandler &owner, MobActor &mob, bool dropLead) {
    if (!isLeashed())
        return;

    Actor *holder = findHolder(owner, mHolderId);
    mHolderId = 0;
    _sync(owner, mob);
    mob.getNavigation().stop(mob);

    if (dropLead) {
        LootDrop drop;
        drop.mIdentifier = LEAD_ITEM;
        const ItemStack lead = LootItems::toItemStack(owner, drop);
        if (!lead.isAir())
            owner.dropItem(owner.getLevelFor(mob), mob.getPosition(), lead, ItemActorHandler::randomDropMotion(),
                           ItemActorHandler::DROP_PICKUP_DELAY);
    }

    const json::Value *leashable = mob.getComponent(LEASHABLE_COMPONENT);
    if (leashable != nullptr)
        EntityEvents::fireTrigger(owner, mob, leashable->get("on_unleash"), holder);
}

void MobLeash::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (!isLeashed())
        return;

    Actor *holder = findHolder(owner, mHolderId);
    if (holder == nullptr || !holder->sharesLevelWith(mob) || mob.getComponent(LEASHABLE_COMPONENT) == nullptr) {
        detach(owner, mob, true);
        return;
    }

    const Preset preset = _preset(owner, mob, *holder);
    const Box mobBox = boxOf(mob, 0.0f);
    const Box holderBox = boxOf(*holder, 0.0f);
    const float dx = holderBox.mCenter.x - mobBox.mCenter.x;
    const float dy = holderBox.mCenter.y - mobBox.mCenter.y;
    const float dz = holderBox.mCenter.z - mobBox.mCenter.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (distance > preset.mMaxDistance) {
        detach(owner, mob, true);
        return;
    }

    if (distance > preset.mSoftDistance) {
        if (distance <= preset.mHardDistance)
            mob.getNavigation().moveTo(holder->getPosition(), FOLLOW_SPEED);
        else
            mob.getNavigation().stop(mob);
    }

    _pull(mob, *holder, preset, distance);
}

MobLeash::Preset MobLeash::_preset(ServerNetworkHandler &owner, MobActor &mob, Actor &holder) const {
    Preset preset{DEFAULT_SOFT_DISTANCE, DEFAULT_HARD_DISTANCE, DEFAULT_MAX_DISTANCE, SpringType::Dampened, 0.0f};

    const json::Value *leashable = mob.getComponent(LEASHABLE_COMPONENT);
    const json::Value *presets = leashable == nullptr ? nullptr : leashable->get("presets");
    if (presets == nullptr || !presets->isArray())
        return preset;

    for (const std::unique_ptr<json::Value> &entry: presets->mArray) {
        const json::Value *filter = entry->get("filter");
        if (filter != nullptr && !EntityFilter::test(*filter, owner, mob, &holder))
            continue;

        preset.mSoftDistance = numberOr(entry.get(), "soft_distance", preset.mSoftDistance);
        preset.mHardDistance = numberOr(entry.get(), "hard_distance", preset.mHardDistance);
        preset.mMaxDistance = numberOr(entry.get(), "max_distance", preset.mMaxDistance);
        preset.mRotationAdjustment = numberOr(entry.get(), "rotation_adjustment", 0.0f);

        const json::Value *spring = entry->get("spring_type");
        const std::string springName = spring == nullptr ? std::string() : spring->string();
        if (springName == "bouncy")
            preset.mSpringType = SpringType::Bouncy;
        else if (springName == "quad_dampened")
            preset.mSpringType = SpringType::QuadDampened;
        else if (springName == "dampened")
            preset.mSpringType = SpringType::Dampened;
        return preset;
    }
    return preset;
}

void MobLeash::_pull(MobActor &mob, const Actor &holder, const Preset &preset, float distance) const {
    Vector3f motion = mob.getMotion();

    if (preset.mSpringType == SpringType::Bouncy) {
        if (distance <= preset.mHardDistance || distance <= 0.0f)
            return;

        const Box mobBox = boxOf(mob, 0.0f);
        const Box holderBox = boxOf(holder, 0.0f);
        motion.x += (holderBox.mCenter.x - mobBox.mCenter.x) / distance * BOUNCY_PULL;
        motion.y += (holderBox.mCenter.y - mobBox.mCenter.y) / distance * BOUNCY_PULL;
        motion.z += (holderBox.mCenter.z - mobBox.mCenter.z) / distance * BOUNCY_PULL;
        mob.setMotion(motion);
        return;
    }

    const bool quad = preset.mSpringType == SpringType::QuadDampened;
    const AttachPoint *mobPoints = quad ? QUAD_POINTS : DAMPENED_MOB_POINTS;
    const AttachPoint *holderPoints = quad ? QUAD_POINTS : DAMPENED_HOLDER_POINTS;
    const size_t count = quad ? 4 : 1;

    const Box mobBox = boxOf(mob, preset.mRotationAdjustment);
    const Box holderBox = boxOf(holder, 0.0f);

    Vector3f force(0.0f, 0.0f, 0.0f);
    for (size_t index = 0; index < count; ++index) {
        const Vector3f from = worldPoint(mobBox, mobPoints[index]);
        const Vector3f to = worldPoint(holderBox, holderPoints[index]);
        const float fx = to.x - from.x;
        const float fy = to.y - from.y;
        const float fz = to.z - from.z;
        const float length = std::sqrt(fx * fx + fy * fy + fz * fz);
        if (length < preset.mHardDistance || length < MIN_DIRECTION_LENGTH)
            continue;

        const float stretch = length - preset.mHardDistance;
        Vector3f pull(fx / length * stretch, fy / length * stretch, fz / length * stretch);
        // The spring pulls fully only while the mob moves away; otherwise it is softened.
        if (pull.x * motion.x + pull.y * motion.y + pull.z * motion.z >= 0.0f)
            pull = Vector3f(pull.x * SPRING_TOWARDS_ONLY, pull.y * SPRING_TOWARDS_ONLY, pull.z * SPRING_TOWARDS_ONLY);
        force = Vector3f(force.x + pull.x, force.y + pull.y, force.z + pull.z);
    }

    if (force.x == 0.0f && force.y == 0.0f && force.z == 0.0f)
        return;

    const float average = 1.0f / (float) count;
    const Vector3f &holderMotion = holder.getMotion();
    motion.x += force.x * average * SPRING_HORIZONTAL + (holderMotion.x - motion.x) * VELOCITY_MATCH;
    motion.y += force.y * average * SPRING_VERTICAL + (holderMotion.y - motion.y) * VELOCITY_MATCH;
    motion.z += force.z * average * SPRING_HORIZONTAL + (holderMotion.z - motion.z) * VELOCITY_MATCH;
    mob.setMotion(motion);
}

void MobLeash::_sync(ServerNetworkHandler &owner, MobActor &mob) {
    mob.getFlags().set(ActorFlag::Leashed, isLeashed());
    owner.syncActorFlags(mob);

    EntityDataEntry entry;
    entry.mId = LEASH_HOLDER_DATA_ID;
    entry.mFormat = EntityDataFormat::Long;
    entry.mLongValue = isLeashed() ? mHolderId : -1;

    EntityDataMap metadata;
    metadata.mEntries.push_back(entry);
    owner.sendActorMetadata(mob, metadata);
}

void MobLeash::saveNbt(Tag &data) const {
    if (isLeashed())
        data.putLong(TAG_LEASHER, mHolderId);
}

void MobLeash::loadNbt(const Tag &data) {
    mHolderId = data.getLong(TAG_LEASHER, 0);
}
