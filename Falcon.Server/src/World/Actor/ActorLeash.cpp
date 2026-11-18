#include "World/Actor/ActorLeash.h"

#include "Core/Json/Json.h"
#include "Core/NBT/Tag.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Definition/EntityEvents.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/RideSystem.h"
#include "World/Level/Storage/Loot/LootItems.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/EntityDataMap.h"

#include <cmath>
#include <memory>
#include <vector>

namespace {
    const char *const TAG_LEASHER = "LeasherID";
    const char *const LEAD_ITEM = "minecraft:lead";
    // how far from the player a held lead may be to move to a leashable_to actor: the default break distance
    const float TIE_RADIUS = 12.0f;

    const int32_t LEASH_HOLDER_DATA_ID = 37;

    // leashable preset defaults, 1.26.52
    const float DEFAULT_SOFT_DISTANCE = 4.0f;
    const float DEFAULT_HARD_DISTANCE = 7.0f;
    const float DEFAULT_MAX_DISTANCE = 12.0f;

    const float FOLLOW_SPEED = 1.0f;
    const float REPATH_DISTANCE_SQUARED = 1.0f;
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

    // local attach points scaled by half extents: the mob's front for the dampened spring, its four lower
    // corners for the quad spring
    const AttachPoint DAMPENED_MOB_POINTS[] = {{0.0f, 0.0f, 1.0f}};
    const AttachPoint DAMPENED_HOLDER_POINTS[] = {{0.0f, 0.0f, 0.0f}};
    const AttachPoint QUAD_POINTS[] = {
            {-1.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, -1.0f}, {1.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 1.0f}};

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
        Actor *holder = RideSystem::resolve(owner, uniqueId);
        if (const ServerPlayer *player = dynamic_cast<const ServerPlayer *>(holder))
            return player->isSpawned() && !player->isDead() ? holder : nullptr;
        return holder != nullptr && holder->isAlive() ? holder : nullptr;
    }
}

void ActorLeash::attach(ServerNetworkHandler &owner, ServerActor &actor, int64_t holderUniqueId) {
    mHolderId = holderUniqueId;
    Actor *holder = findHolder(owner, holderUniqueId);
    mHeldByActor = holder != nullptr && !holder->isPlayer();
    const ServerActor *holderActor = dynamic_cast<const ServerActor *>(holder);
    const json::Value *to = holderActor == nullptr ? nullptr : holderActor->getLeashableToComponent();
    const json::Value *removal = to == nullptr ? nullptr : to->get("unleash_on_removal");
    mKeptPastRemoval = removal != nullptr && !removal->boolean(true);
    _sync(owner, actor);

    const json::Value *leashable = actor.getLeashableComponent();
    if (MobActor *mob = dynamic_cast<MobActor *>(&actor); mob != nullptr && leashable != nullptr)
        EntityEvents::fireTrigger(owner, *mob, leashable->get("on_leash"), holder);
}

void ActorLeash::detach(ServerNetworkHandler &owner, ServerActor &actor, bool dropLead) {
    if (!isLeashed())
        return;

    Actor *holder = findHolder(owner, mHolderId);
    mHolderId = 0;
    mHeldByActor = false;
    _sync(owner, actor);
    MobActor *mob = dynamic_cast<MobActor *>(&actor);
    if (mob != nullptr)
        mob->getNavigation().stop(*mob);

    if (dropLead) {
        LootDrop drop;
        drop.mIdentifier = LEAD_ITEM;
        const ItemStack lead = LootItems::toItemStack(owner, drop);
        if (!lead.isAir())
            owner.dropItem(owner.getLevelFor(actor), actor.getPosition(), lead, ItemActorHandler::randomDropMotion(),
                           ItemActorHandler::DROP_PICKUP_DELAY);
    }

    const json::Value *leashable = actor.getLeashableComponent();
    if (mob != nullptr && leashable != nullptr)
        EntityEvents::fireTrigger(owner, *mob, leashable->get("on_unleash"), holder);
}

void ActorLeash::tick(ServerNetworkHandler &owner, ServerActor &actor) {
    if (!isLeashed())
        return;

    Actor *holder = findHolder(owner, mHolderId);
    // an offline player keeps the leash: it reconnects when the player rejoins
    if (holder == nullptr) {
        if (mHeldByActor && !mKeptPastRemoval)
            detach(owner, actor, true);
        return;
    }
    mHeldByActor = !holder->isPlayer();

    if (!holder->sharesLevelWith(actor) || actor.getLeashableComponent() == nullptr) {
        detach(owner, actor, true);
        return;
    }

    const Preset preset = _preset(owner, actor, *holder);
    const Box mobBox = boxOf(actor, 0.0f);
    const Box holderBox = boxOf(*holder, 0.0f);
    const float dx = holderBox.mCenter.x - mobBox.mCenter.x;
    const float dy = holderBox.mCenter.y - mobBox.mCenter.y;
    const float dz = holderBox.mCenter.z - mobBox.mCenter.z;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (distance > preset.mMaxDistance) {
        detach(owner, actor, true);
        return;
    }

    MobActor *mob = dynamic_cast<MobActor *>(&actor);
    if (mob != nullptr && distance > preset.mSoftDistance) {
        if (distance <= preset.mHardDistance) {
            const Vector3f &target = holder->getPosition();
            const float moved = (target.x - mPathTarget.x) * (target.x - mPathTarget.x)
                                + (target.y - mPathTarget.y) * (target.y - mPathTarget.y)
                                + (target.z - mPathTarget.z) * (target.z - mPathTarget.z);
            // re-pathing every tick restarts the search and stalls the mob
            if (mob->getNavigation().isDone() || moved > REPATH_DISTANCE_SQUARED) {
                mPathTarget = target;
                mob->getNavigation().moveTo(target, FOLLOW_SPEED);
            }
        } else
            mob->getNavigation().stop(*mob);
    }

    _pull(actor, *holder, preset, distance);
}

bool ActorLeash::tieHeldLeads(ServerNetworkHandler &owner, ServerPlayer &player, ServerActor &holder) {
    if (holder.getLeashableToComponent() == nullptr)
        return false;

    std::vector<ServerActor *> led;
    owner.forEachActorNear(player.getPosition(), TIE_RADIUS, [&](ServerActor &actor) {
        const ActorLeash *leash = actor.getActorLeash();
        if (&actor != &holder && leash != nullptr && leash->getHolderId() == player.getUniqueId()
            && actor.sharesLevelWith(player))
            led.push_back(&actor);
    });
    // a holder tied to the player keeps its own lead: it would hold itself
    for (ServerActor *actor: led)
        actor->getActorLeash()->attach(owner, *actor, holder.getUniqueId());
    return !led.empty();
}

ActorLeash::Preset ActorLeash::_preset(ServerNetworkHandler &owner, ServerActor &actor, Actor &holder) const {
    Preset preset{DEFAULT_SOFT_DISTANCE, DEFAULT_HARD_DISTANCE, DEFAULT_MAX_DISTANCE, SpringType::Dampened, 0.0f};

    const json::Value *leashable = actor.getLeashableComponent();
    const json::Value *presets = leashable == nullptr ? nullptr : leashable->get("presets");
    if (presets == nullptr || !presets->isArray())
        return preset;

    for (const std::unique_ptr<json::Value> &entry: presets->mArray) {
        const json::Value *filter = entry->get("filter");
        if (filter != nullptr && !EntityFilter::test(*filter, owner, actor, &holder))
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

void ActorLeash::_pull(ServerActor &actor, const Actor &holder, const Preset &preset, float distance) const {
    Vector3f motion = actor.getMotion();

    if (preset.mSpringType == SpringType::Bouncy) {
        if (distance <= preset.mHardDistance || distance <= 0.0f)
            return;

        const Box mobBox = boxOf(actor, 0.0f);
        const Box holderBox = boxOf(holder, 0.0f);
        motion.x += (holderBox.mCenter.x - mobBox.mCenter.x) / distance * BOUNCY_PULL;
        motion.y += (holderBox.mCenter.y - mobBox.mCenter.y) / distance * BOUNCY_PULL;
        motion.z += (holderBox.mCenter.z - mobBox.mCenter.z) / distance * BOUNCY_PULL;
        actor.setMotion(motion);
        return;
    }

    const bool quad = preset.mSpringType == SpringType::QuadDampened;
    const AttachPoint *mobPoints = quad ? QUAD_POINTS : DAMPENED_MOB_POINTS;
    const AttachPoint *holderPoints = quad ? QUAD_POINTS : DAMPENED_HOLDER_POINTS;
    const size_t count = quad ? 4 : 1;

    const Box mobBox = boxOf(actor, preset.mRotationAdjustment);
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
        // full pull only while the mob moves away; softened otherwise
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
    actor.setMotion(motion);
}

void ActorLeash::_sync(ServerNetworkHandler &owner, ServerActor &actor) {
    actor.getFlags().set(ActorFlag::Leashed, isLeashed());
    owner.syncActorFlags(actor);

    EntityDataEntry entry;
    entry.mId = LEASH_HOLDER_DATA_ID;
    entry.mFormat = EntityDataFormat::Long;
    entry.mLongValue = isLeashed() ? mHolderId : -1;

    EntityDataMap metadata;
    metadata.mEntries.push_back(entry);
    owner.sendActorMetadata(actor, metadata);
}

void ActorLeash::appendSpawnMetadata(EntityDataMap &metadata) const {
    if (!isLeashed())
        return;

    EntityDataEntry entry;
    entry.mId = LEASH_HOLDER_DATA_ID;
    entry.mFormat = EntityDataFormat::Long;
    entry.mLongValue = mHolderId;
    metadata.mEntries.push_back(entry);
}

void ActorLeash::saveNbt(Tag &data) const {
    if (isLeashed())
        data.putLong(TAG_LEASHER, mHolderId);
}

void ActorLeash::loadNbt(const Tag &data) {
    mHolderId = data.getLong(TAG_LEASHER, 0);
}
