#include "World/Actor/Vehicle/BoatActor.h"

#include "Core/Json/Json.h"
#include "Core/Math/AxisAlignedBB.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/CorrectPlayerMovePredictionPacket.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorCollisionSystem.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Actor/Movement/BuoyancySystem.h"
#include "World/Actor/Movement/PhysicsComponent.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/PlayerInventory.h"
#include "Protocol/Types/ItemDefinition.h"
#include "World/Actor/RideSystem.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <vector>

FALCON_REGISTER_ACTOR(BoatActor, BoatActor::BOAT);

static const ActorClassRegistry::Registration gActorRegistrationChestBoat(
        BoatActor::CHEST_BOAT, [](uint64_t runtimeId, const std::string &identifier) -> std::unique_ptr<ServerActor> {
            return std::make_unique<BoatActor>(runtimeId, identifier);
        });

namespace {
    constexpr int32_t ACTOR_DATA_VARIANT = 2;
    constexpr int32_t ACTOR_DATA_HURT_TIME = 11;
    constexpr int32_t ACTOR_DATA_HURT_DIRECTION = 12;

    // by variant, as the game numbers its boat items
    const std::array<const char *, 11> WOODS = {"oak",      "spruce", "birch",  "jungle",   "acacia", "dark_oak",
                                                "mangrove", "bamboo", "cherry", "pale_oak", "poplar"};

    const char *const TAG_VARIANT = "Variant";
    const char *const DROP_RULE = "doentitydrops";
    const char *const RIDEABLE = "minecraft:rideable";
    const char *const BUOYANT = "minecraft:buoyant";
    const char *const LEASHABLE = "minecraft:leashable";
    const char *const LEASHABLE_TO = "minecraft:leashable_to";
    const char *const PUSHABLE_BY_ENTITY = "minecraft:pushable_by_entity";
    const char *const LEAD_ITEM = "minecraft:lead";


    // minecraft:hurt_on_condition: lava, 4 a tick
    const float LAVA_DAMAGE = 4.0f;
    // minecraft:timer of the column down group: the boat sinks after three seconds over it
    const int64_t SINK_TICKS = 3 * 20;

    // the momentum of the friction system on water and over it
    const float WATER_MOMENTUM = 0.9f;
    // the move control stops once the boat stayed this long under water
    const int32_t MAX_SUBMERGED_TICKS = 24;
    // a paddle's force becomes this much speed; each paddle turns the boat by three times its push
    const float PADDLE_SCALE = 0.01375f;
    const float PADDLE_TURN = 3.0f;
    const float PADDLE_TIME_WRAP = 1000.0f;
    // a boat barely moving turns faster and creeps forward
    const float STILL_SPEED_SQUARED = 0.010000001f;
    const float STILL_TURN_BOOST = 1.6f;
    const float TURN_SCALE = 10.0f;
    // the paddle input: a fresh stroke is 3, a held one settles to 2.5, a released one halves away
    const float STROKE_FORCE = 3.0f;
    const float HELD_FORCE = 2.5f;
    const float HELD_FADE = 0.05f;
    const float ANALOG_EASE = 0.1f;
    const float RELEASED_FADE = 0.5f;
    const float RELEASED_STOP = 0.01f;
    const int64_t STROKE_TICKS = 10;
    // backing up paddles at this share of the forward strength
    const float REVERSE_SCALE = -0.15f;
    // how far the rider's own prediction may stray from the server's before the server corrects it
    const float PREDICTION_TOLERANCE = 1.0f;

    const float HURT_SCALE = 10.0f;
    const float BREAK_DAMAGE = 40.0f;
    const int32_t HURT_TICKS = 10;

    // the most a steering rider may move the boat in one input: a boat on blue ice, a fall, a rise out of water
    const float MAX_CONTROLLED_HORIZONTAL = 4.0f;
    const float MAX_CONTROLLED_FALL = 4.0f;
    const float MAX_CONTROLLED_RISE = 1.0f;
    // the box is checked slightly shrunk: resting on a block is not inside it
    const float CONTROLLED_BOX_SHRINK = 0.01f;
    // the pull in reach around the boat
    const float PULL_IN_MARGIN = 0.2f;
    const float DEGREES_TO_RADIANS = 0.017453292519943295f;

    const json::Value *componentsOf(const std::string &identifier) {
        const json::Value *definition = EntityDefinitions::find(identifier);
        return definition == nullptr ? nullptr : definition->get("components");
    }

    const json::Value *groupComponent(const std::string &identifier, const char *group, const char *component) {
        const json::Value *definition = EntityDefinitions::find(identifier);
        const json::Value *groups = definition == nullptr ? nullptr : definition->get("component_groups");
        const json::Value *found = groups == nullptr ? nullptr : groups->get(group);
        return found == nullptr ? nullptr : found->get(component);
    }

    float numberIn(const json::Value *object, const char *key, float fallback) {
        const json::Value *value = object == nullptr ? nullptr : object->get(key);
        return value == nullptr ? fallback : (float) value->number(fallback);
    }

    Vector3f rotated(const Vector3f &seat, float yaw) {
        const float radians = yaw * DEGREES_TO_RADIANS;
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        return Vector3f(seat.x * cosine - seat.z * sine, seat.y, seat.x * sine + seat.z * cosine);
    }

    bool isWater(const BlockState *state) {
        return state != nullptr && LiquidView(*state).isWater();
    }

    // the shapes come back with the row under the area: only the ones reaching into it count
    bool collides(Level &level, const AxisAlignedBB &box, const Actor *actor) {
        for (const AxisAlignedBB &shape: level.getCollisionBoxes(box, actor)) {
            if (shape.intersectsWith(box))
                return true;
        }
        return false;
    }

    std::mt19937 &waveRandom() {
        static std::mt19937 random(std::random_device{}());
        return random;
    }
}

BoatActor::BoatActor(uint64_t runtimeId, const std::string &identifier) : ServerActor(runtimeId, identifier) {
    if (identifier == CHEST_BOAT)
        mChest = std::make_unique<VehicleChest>(identifier);
}

int32_t BoatActor::variantOfItem(const std::string &item) {
    if (item == "minecraft:boat" || item == "minecraft:chest_boat")
        return 0;
    if (item == "minecraft:bamboo_raft" || item == "minecraft:bamboo_chest_raft")
        return RAFT_VARIANT;
    for (size_t variant = 0; variant < WOODS.size(); ++variant) {
        const std::string wood = std::string("minecraft:") + WOODS[variant];
        if (item == wood + "_boat" || item == wood + "_chest_boat")
            return (int32_t) variant;
    }
    return -1;
}

std::string BoatActor::itemOf(int32_t variant, bool chest) {
    if (variant == RAFT_VARIANT)
        return chest ? "minecraft:bamboo_chest_raft" : "minecraft:bamboo_raft";
    const char *wood = variant >= 0 && (size_t) variant < WOODS.size() ? WOODS[(size_t) variant] : WOODS[0];
    return std::string("minecraft:") + wood + (chest ? "_chest_boat" : "_boat");
}

ActorSize BoatActor::getSize() const {
    return ActorSize{WIDTH, HEIGHT};
}

// the seats of the can_ride group the spawn event adds by variant; a sunk boat has none
size_t BoatActor::_seatCount() const {
    if (mWater == Water::Sunk)
        return 0;
    const json::Value *rideable = groupComponent(
            getTypeId(), mVariant == RAFT_VARIANT ? "minecraft:can_ride_bamboo" : "minecraft:can_ride_default", RIDEABLE);
    return (size_t) std::max(0.0f, numberIn(rideable, "seat_count", 1.0f));
}

Vector3f BoatActor::getSeatOffset(size_t index, size_t passengerCount) const {
    const json::Value *rideable = groupComponent(
            getTypeId(), mVariant == RAFT_VARIANT ? "minecraft:can_ride_bamboo" : "minecraft:can_ride_default", RIDEABLE);
    const json::Value *seats = rideable == nullptr ? nullptr : rideable->get("seats");
    if (seats == nullptr || !seats->isArray())
        return ServerActor::getSeatOffset(index, passengerCount);

    size_t applicable = 0;
    for (const std::unique_ptr<json::Value> &seat: seats->mArray) {
        const int32_t minimum = (int32_t) numberIn(seat.get(), "min_rider_count", 0.0f);
        const int32_t maximum = (int32_t) numberIn(seat.get(), "max_rider_count", (float) passengerCount);
        if ((int32_t) passengerCount < minimum || (int32_t) passengerCount > maximum)
            continue;
        if (applicable++ != index)
            continue;

        const json::Value *position = seat->get("position");
        if (position == nullptr || !position->isArray() || position->mArray.size() < 3)
            break;
        return Vector3f((float) position->mArray[0]->number(0.0), (float) position->mArray[1]->number(0.0),
                        (float) position->mArray[2]->number(0.0));
    }
    return ServerActor::getSeatOffset(index, passengerCount);
}

bool BoatActor::_canSeat(float riderWidth) const {
    if (mExpired || getPassengers().size() >= _seatCount())
        return false;
    const json::Value *rideable = componentsOf(getTypeId());
    rideable = rideable == nullptr ? nullptr : rideable->get(RIDEABLE);
    // passenger_max_width 0 lets any width ride
    const float maxWidth = numberIn(rideable, "passenger_max_width", 0.0f);
    return maxWidth <= 0.0f || riderWidth <= maxWidth;
}

Container *BoatActor::getEntityContainer() {
    return mChest == nullptr ? nullptr : &mChest->getContainer();
}

// a chest boat opens its chest on a sneaking click; any other click boards
bool BoatActor::onInteract(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (mExpired)
        return false;
    if (ActorLeash::tieHeldLeads(owner, player, *this) || _useLead(owner, player))
        return true;
    if (mChest != nullptr && player.getFlags().get(ActorFlag::Sneaking))
        return mChest->open(owner, player, *this);
    if (player.isRiding() || player.getFlags().get(ActorFlag::Sneaking)
        || !_canSeat(ActorClassRegistry::getSize("minecraft:player").mWidth))
        return false;
    return RideSystem::mount(owner, player, *this, true);
}

const json::Value *BoatActor::getLeashableComponent() const {
    const json::Value *components = componentsOf(getTypeId());
    return components == nullptr ? nullptr : components->get(LEASHABLE);
}

const json::Value *BoatActor::getLeashableToComponent() const {
    const json::Value *components = componentsOf(getTypeId());
    return components == nullptr ? nullptr : components->get(LEASHABLE_TO);
}

bool BoatActor::_useLead(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (getLeashableComponent() == nullptr)
        return false;

    if (mLeash.isLeashed()) {
        if (mLeash.getHolderId() != player.getUniqueId())
            return false;
        mLeash.detach(owner, *this, !player.isCreative());
        return true;
    }

    const ItemStack &held = player.getInventory().getItemInHand();
    if (held.mDefinition == nullptr || held.mDefinition->getIdentifier() != LEAD_ITEM)
        return false;
    mLeash.attach(owner, *this, player.getUniqueId());
    player.consumeOneHeldItem();
    return true;
}

Vector3f BoatActor::_push(ServerNetworkHandler &owner) {
    const json::Value *components = componentsOf(getTypeId());
    mPushPresets.refresh(components == nullptr ? nullptr : components->get(PUSHABLE_BY_ENTITY));
    Vector3f total(0.0f, 0.0f, 0.0f);
    if (mPushPresets.mPresets.empty())
        return total;

    const AxisAlignedBB self = ActorPushSystem::boundingBoxOf(*this);
    const auto pushBy = [&](Actor &pusher, float pusherWidth) {
        // riders and their vehicle never push each other; a sneaking pusher does not push
        if (pusher.getVehicleId() == getUniqueId() || pusher.getUniqueId() == getVehicleId()
            || pusher.getFlags().get(ActorFlag::Sneaking)
            || !ActorPushSystem::boundingBoxOf(pusher).intersectsWith(self))
            return;

        const PushableByEntityPreset *preset = PushableByEntity::findPreset(owner, *this, mPushPresets, pusher);
        if (preset == nullptr || preset->mMode != PushMode::LegacyBoat)
            return;
        const float offsetX = pusher.getPosition().x - getPosition().x;
        const float offsetZ = pusher.getPosition().z - getPosition().z;
        if (!PushableByEntity::inReach(*preset, offsetX, offsetZ, pusherWidth))
            return;

        const Vector3f push = PushableByEntity::legacyBoatPush(*preset, offsetX, offsetZ);
        total = Vector3f(total.x + push.x, total.y, total.z + push.z);

        // the pushed actor takes its share unless its own preset refuses this boat; a player moves itself
        ServerActor *other = dynamic_cast<ServerActor *>(&pusher);
        if (other == nullptr)
            return;
        const MobActor *mob = dynamic_cast<const MobActor *>(other);
        const PushableByEntityPreset *refused =
                mob == nullptr ? nullptr : PushableByEntity::findPreset(owner, *mob, mob->getPushPresets(), *this);
        if (refused != nullptr && refused->mMode == PushMode::None)
            return;
        const Vector3f motion = other->getMotion();
        other->setMotion(Vector3f(motion.x - push.x * preset->mPushScaleOther, motion.y,
                                  motion.z - push.z * preset->mPushScaleOther));
    };

    Level &level = owner.getLevelFor(*this);
    owner.forEachActorNear(getPosition(), WIDTH + owner.getWidestActorHalfWidth(), [&](ServerActor &actor) {
        if (&actor != this && actor.isAlive() && !actor.isProjectile() && actor.isPushable()
            && &owner.getLevelFor(actor) == &level)
            pushBy(actor, actor.getSize().mWidth);
    });
    owner.forEachPlayerNear(getPosition(), WIDTH, [&](ServerPlayer &player) {
        if (player.isSpawned() && !player.isDead() && !player.isSpectator() && player.sharesLevelWith(*this))
            pushBy(player, ActorClassRegistry::getSize("minecraft:player").mWidth);
    });
    return total;
}

bool BoatActor::onHurt(ServerNetworkHandler &owner, float amount, ServerPlayer *source) {
    _damage(owner, amount, source);
    return true;
}

void BoatActor::_damage(ServerNetworkHandler &owner, float amount, ServerPlayer *source) {
    if (mExpired)
        return;
    if (source != nullptr && source->isCreative()) {
        _destroy(owner, false);
        return;
    }

    mHurtDirection = -mHurtDirection;
    mHurtTime = HURT_TICKS;
    mDamage += amount * HURT_SCALE;
    _sendHurt(owner);
    if (mDamage > BREAK_DAMAGE)
        _destroy(owner, owner.getLevelFor(*this).getGameRules().getBool(DROP_RULE));
}

void BoatActor::_sendHurt(ServerNetworkHandler &owner) {
    EntityDataMap metadata;
    EntityDataEntry hurtTime;
    hurtTime.mId = ACTOR_DATA_HURT_TIME;
    hurtTime.mFormat = EntityDataFormat::Int;
    hurtTime.mIntValue = mHurtTime;
    metadata.mEntries.push_back(hurtTime);

    EntityDataEntry direction;
    direction.mId = ACTOR_DATA_HURT_DIRECTION;
    direction.mFormat = EntityDataFormat::Int;
    direction.mIntValue = mHurtDirection;
    metadata.mEntries.push_back(direction);
    owner.sendActorMetadata(*this, metadata);
}

void BoatActor::_destroy(ServerNetworkHandler &owner, bool drop) {
    if (mExpired)
        return;
    mExpired = true;
    RideSystem::ejectAll(owner, *this);
    mLeash.detach(owner, *this, true);
    if (mChest != nullptr)
        mChest->dropAll(owner, *this);
    if (drop)
        owner.spawnItemActor(owner.getLevelFor(*this), itemOf(mVariant, mChest != nullptr), 1, getPosition());
}

ServerPlayer *BoatActor::_controller(ServerNetworkHandler &owner) const {
    if (getPassengers().empty())
        return nullptr;
    return dynamic_cast<ServerPlayer *>(RideSystem::resolve(owner, getPassengers().front()));
}

void BoatActor::followController(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3f &feetPosition,
                                 const PlayerAuthInputPacket &packet) {
    BoatActor *boat = dynamic_cast<BoatActor *>(RideSystem::resolve(owner, player.getVehicleId()));
    if (boat == nullptr || boat->mExpired || boat->getPassengers().empty()
        || boat->getPassengers().front() != player.getUniqueId())
        return;

    const float yaw = packet.mHasVehicleRotation ? packet.mVehicleRotationY : boat->getRotation().y;
    const bool finite = std::isfinite(yaw) && std::isfinite(feetPosition.x) && std::isfinite(feetPosition.y)
                        && std::isfinite(feetPosition.z);

    // the server runs the same input through the boat systems, then keeps the rider's own result when close
    const Vector3f before = boat->getPosition();
    boat->_readRider(packet);
    boat->_paddle();
    boat->_step(owner, true);
    if (!finite) {
        boat->_correct(owner, player, packet.mTick);
        return;
    }

    const Vector3f seat = rotated(boat->getSeatOffset(0, boat->getPassengers().size()), yaw);
    const Vector3f predicted(feetPosition.x - seat.x, feetPosition.y - seat.y, feetPosition.z - seat.z);
    const float dx = predicted.x - before.x;
    const float dy = predicted.y - before.y;
    const float dz = predicted.z - before.z;
    const float half = WIDTH * 0.5f - CONTROLLED_BOX_SHRINK;
    const AxisAlignedBB box(predicted.x - half, predicted.y + CONTROLLED_BOX_SHRINK, predicted.z - half,
                            predicted.x + half, predicted.y + HEIGHT - CONTROLLED_BOX_SHRINK, predicted.z + half);
    const Vector3f &simulated = boat->getPosition();
    const float sx = predicted.x - simulated.x;
    const float sy = predicted.y - simulated.y;
    const float sz = predicted.z - simulated.z;
    if (dx * dx + dz * dz > MAX_CONTROLLED_HORIZONTAL * MAX_CONTROLLED_HORIZONTAL || dy < -MAX_CONTROLLED_FALL
        || dy > MAX_CONTROLLED_RISE || collides(owner.getLevelFor(*boat), box, boat)
        || sx * sx + sy * sy + sz * sz > PREDICTION_TOLERANCE * PREDICTION_TOLERANCE) {
        boat->_correct(owner, player, packet.mTick);
        return;
    }

    boat->setPosition(predicted);
    boat->setRotation(Vector3f(boat->getRotation().x, yaw, yaw));
}

// the boat as the server ran it: the rider rewinds to it and replays its inputs from there
void BoatActor::_correct(ServerNetworkHandler &owner, ServerPlayer &player, int64_t tick) {
    CorrectPlayerMovePredictionPacket correction;
    correction.mPredictionType = PredictionType::Vehicle;
    correction.mPosition = getPosition();
    correction.mPosition.y += getBaseOffset();
    correction.mDelta = getMotion();
    correction.mVehicleRotation = Vector2f(getRotation().x, getRotation().y);
    correction.mHasVehicleAngularVelocity = true;
    correction.mVehicleAngularVelocity = mAngularVelocity;
    correction.mOnGround = isOnGround();
    correction.mTick = (uint64_t) tick;
    owner.getNetworkHandler().send(player.getNetworkIdentifier(), correction, owner.getCodecContext());
}

// the passenger paddle input: keyboard, gamepad and the touch stick steer by the move vector; the touch paddle
// buttons press each paddle on its own
void BoatActor::_readRider(const PlayerAuthInputPacket &packet) {
    const bool left = packet.hasInputFlag((int32_t) PlayerAuthInputData::PaddleLeft);
    const bool right = packet.hasInputFlag((int32_t) PlayerAuthInputData::PaddleRight);
    mRider.mAnalog = packet.mInputMode != PlayerInputMode::Touch || !(left || right);
    mRider.mPressed = {left, right};
    mRider.mAnalogForce = {0.0f, 0.0f};
    if (!mRider.mAnalog)
        return;

    const float forward = std::clamp(packet.mMotionY, -1.0f, 1.0f);
    const float strafe = packet.mMotionX;
    float magnitude = std::sqrt(forward * forward + strafe * strafe);
    float turn = strafe;
    if (forward < 0.0f) {
        magnitude *= REVERSE_SCALE;
        turn = -strafe;
    }
    const float leftShare = turn > 1.0f ? 0.0f : turn > 0.0f ? 1.0f - turn : 1.0f;
    const float rightShare = turn > 0.0f ? 1.0f : turn > -1.0f ? turn + 1.0f : 1.0f;
    mRider.mAnalogForce = {magnitude * leftShare, magnitude * rightShare};
}

// the vehicle paddle input: the force each paddle pushes with this tick
void BoatActor::_paddle() {
    for (size_t side = 0; side < mPaddles.size(); ++side) {
        Paddle &paddle = mPaddles[side];
        if (mRider.mAnalog) {
            const float input = mRider.mAnalogForce[side];
            const float stroke = STROKE_FORCE * input;
            if (input == 0.0f) {
                paddle.mForce = 0.0f;
            } else if (mPaddleTick - paddle.mPressTick >= STROKE_TICKS) {
                paddle.mForce = stroke;
                paddle.mPressTick = mPaddleTick;
            } else {
                const float eased = std::max(0.0f, std::fabs(stroke) - ANALOG_EASE);
                paddle.mForce = input < 0.0f ? -eased : eased;
            }
        } else if (mRider.mPressed[side]) {
            if (paddle.mPressTick >= 0) {
                paddle.mForce = mPaddleTick - paddle.mPressTick >= STROKE_TICKS
                                        ? std::max(HELD_FORCE, paddle.mForce - HELD_FADE)
                                        : STROKE_FORCE;
            } else {
                paddle.mForce = mPaddleTick - paddle.mReleaseTick < STROKE_TICKS
                                        ? std::max(HELD_FORCE, paddle.mForce - HELD_FADE)
                                        : STROKE_FORCE;
                paddle.mTime = 0.0f;
                paddle.mPressTick = mPaddleTick;
            }
        } else {
            if (paddle.mPressTick >= 0) {
                paddle.mReleaseTick = paddle.mPressTick;
                paddle.mPressTick = -1;
            }
            paddle.mForce = paddle.mForce <= RELEASED_STOP ? 0.0f : paddle.mForce * RELEASED_FADE;
        }
    }
    ++mPaddleTick;
}

// the move control: the paddles push the boat forward and turn it, left paddle to the right
void BoatActor::_steer(Vector3f &motion, float momentum) {
    if (mSubmergedTicks > MAX_SUBMERGED_TICKS)
        return;

    float forward = 0.0f;
    float turn = 0.0f;
    for (size_t side = 0; side < mPaddles.size(); ++side) {
        Paddle &paddle = mPaddles[side];
        const float push = paddle.mForce * PADDLE_SCALE;
        if (push == 0.0f) {
            paddle.mTime = 0.0f;
            paddle.mPressTick = -1;
            continue;
        }
        if (!mOutOfControl) {
            forward += push;
            turn += side == 0 ? PADDLE_TURN * push : -PADDLE_TURN * push;
        }
        paddle.mTime += push;
        if (paddle.mTime > PADDLE_TIME_WRAP)
            paddle.mTime -= PADDLE_TIME_WRAP;
    }

    if (motion.x * motion.x + motion.z * motion.z < STILL_SPEED_SQUARED && turn != 0.0f) {
        forward *= momentum;
        turn *= STILL_TURN_BOOST;
    }
    mAngularVelocity = (turn * TURN_SCALE + mAngularVelocity) * momentum;
    const float yaw = getRotation().y + mAngularVelocity;
    setRotation(Vector3f(getRotation().x, yaw, yaw));

    const float heading = (90.0f - yaw) * DEGREES_TO_RADIANS;
    motion.x = (motion.x + forward * std::sin(heading)) * momentum;
    motion.z = (motion.z + forward * std::cos(heading)) * momentum;
}

void BoatActor::tick(ServerNetworkHandler &owner) {
    if (mExpired)
        return;
    if (mHurtTime > 0)
        --mHurtTime;
    if (mDamage > 0.0f)
        mDamage = std::max(0.0f, mDamage - 1.0f);

    _tickWater(owner);
    if (mExpired)
        return;
    mLeash.tick(owner, *this);
    ServerActor::tick(owner);
    if (!mExpired)
        _pullInMobs(owner);
}

// the bubble column notifier events: up holds the boat out of control, down sinks it after its timer
void BoatActor::_tickWater(ServerNetworkHandler &owner) {
    Level &level = owner.getLevelFor(*this);
    const LiquidContact feet = LiquidBlocksFetch::at(level, getPosition());
    if (feet.lava)
        hurt(owner, LAVA_DAMAGE, ActorDamageSource::environment("death.attack.lava", getName()));
    if (mExpired || mWater == Water::Sunk)
        return;

    const Water column = !feet.bubble ? Water::Floating : feet.dragDown ? Water::ColumnDown : Water::ColumnUp;
    if (column != mWater) {
        mWater = column;
        mSinkTick = column == Water::ColumnDown ? owner.getCurrentTick() + SINK_TICKS : -1;
    }
    if (mWater == Water::ColumnDown && owner.getCurrentTick() >= mSinkTick) {
        mWater = Water::Sunk;
        RideSystem::ejectAll(owner, *this);
    }
}

// pull_in_entities: a mob bumping a boat with a free seat climbs in, unless a player steers it
void BoatActor::_pullInMobs(ServerNetworkHandler &owner) {
    if (getPassengers().size() >= _seatCount() || _controller(owner) != nullptr)
        return;

    const AxisAlignedBB box = ActorPushSystem::boundingBoxOf(*this);
    const AxisAlignedBB reach(box.mMinX - PULL_IN_MARGIN, box.mMinY, box.mMinZ - PULL_IN_MARGIN,
                              box.mMaxX + PULL_IN_MARGIN, box.mMaxY, box.mMaxZ + PULL_IN_MARGIN);
    Level &level = owner.getLevelFor(*this);
    MobActor *rider = nullptr;
    owner.forEachActorNear(getPosition(), WIDTH + PULL_IN_MARGIN, [&](ServerActor &actor) {
        MobActor *mob = dynamic_cast<MobActor *>(&actor);
        if (rider != nullptr || mob == nullptr || !mob->isAlive() || mob->isRiding() || mob->hasPassengers()
            || &owner.getLevelFor(*mob) != &level || !_canSeat(mob->getSize().mWidth))
            return;
        if (ActorPushSystem::boundingBoxOf(*mob).intersectsWith(reach))
            rider = mob;
    });
    if (rider != nullptr)
        RideSystem::mount(owner, *rider, *this, false);
}

// the boat friction system: water and its surface keep 0.9 of the speed, ground its block's friction, air all of
// it; a boat out of water and off the ground cannot be steered
float BoatActor::_friction(Level &level) {
    const Vector3f position = getPosition();
    const int32_t x = (int32_t) std::floor(position.x);
    const int32_t y = (int32_t) std::floor(position.y);
    const int32_t z = (int32_t) std::floor(position.z);
    // the bubble column groups hold the boat out_of_control
    const bool column = mWater == Water::ColumnUp || mWater == Water::ColumnDown;

    BuoyancySettings water;
    water.mEnabled = mWater != Water::Sunk;
    const BuoyancyContact contact = BuoyancySystem::contact(level, position, water);
    if (contact.mSurface || contact.mSubmerged) {
        mSubmergedTicks = contact.mSurface ? 0 : mSubmergedTicks + 1;
        mOutOfControl = column;
        return WATER_MOMENTUM;
    }

    mOutOfControl = true;
    const BlockState *at = level.peekBlockPtr(x, y, z);
    if (at != nullptr && at->mName != BlockNames::AIR) {
        if (isWater(at)) {
            mOutOfControl = column;
            return WATER_MOMENTUM;
        }
        if (!isOnGround())
            return 1.0f;
        mOutOfControl = column;
        // a block without a shape at the boat's feet lends the friction of the one under it
        const AxisAlignedBB cell((float) x, (float) y, (float) z, (float) x + 1.0f, (float) y + 1.0f, (float) z + 1.0f);
        if (!collides(level, cell, this)) {
            const BlockState *below = level.peekBlockPtr(x, y - 1, z);
            return below == nullptr ? 1.0f : Block(*below).getFrictionFactor();
        }
        return Block(*at).getFrictionFactor();
    }

    const BlockState *below = level.peekBlockPtr(x, y - 1, z);
    if (below == nullptr)
        return 1.0f;
    if (isOnGround()) {
        mOutOfControl = column;
        return Block(*below).getFrictionFactor();
    }
    if (!isWater(below))
        return 1.0f;
    mOutOfControl = column;
    return WATER_MOMENTUM;
}

void BoatActor::tickMovement(ServerNetworkHandler &owner) {
    // a steering player's inputs move the boat as they arrive
    if (ServerPlayer *controller = _controller(owner)) {
        RideSystem::syncPassengerPositions(owner, *this);
        if (needsMovementSync()) {
            owner.broadcastActorMove(*this, false, controller);
            markMovementSynced();
        }
        return;
    }

    for (Paddle &paddle: mPaddles)
        paddle.mForce = 0.0f;
    _step(owner, false);
    RideSystem::syncPassengerPositions(owner, *this);
    if (needsMovementSync()) {
        owner.broadcastActorMove(*this);
        markMovementSynced();
    }
}

void BoatActor::_step(ServerNetworkHandler &owner, bool steered) {
    Level &level = owner.getLevelFor(*this);
    const Vector3f push = _push(owner);
    Vector3f motion = getMotion();
    motion.x += push.x;
    motion.z += push.z;
    const float momentum = _friction(level);
    motion.x *= momentum;
    motion.z *= momentum;
    mAngularVelocity *= momentum;
    if (steered)
        _steer(motion, momentum);

    AxisAlignedBB box = ActorPushSystem::boundingBoxOf(*this);
    const ActorMoveResult result = ActorCollisionSystem::move(level, box, motion, 0.0f, isOnGround(), this);
    setPosition(Vector3f((box.mMinX + box.mMaxX) * 0.5f, box.mMinY, (box.mMinZ + box.mMaxZ) * 0.5f));
    if (result.mCollidedX)
        motion.x = 0.0f;
    if (result.mCollidedY)
        motion.y = 0.0f;
    if (result.mCollidedZ)
        motion.z = 0.0f;
    setOnGround(result.mOnGround);

    BuoyancySettings buoyancy;
    if (mWater != Water::Sunk) {
        const char *group = mWater == Water::Floating   ? "minecraft:floating"
                            : mWater == Water::ColumnUp ? "minecraft:above_bubble_column_up"
                                                        : "minecraft:above_bubble_column_down";
        buoyancy = BuoyancySystem::read(groupComponent(getTypeId(), group, BUOYANT));
    }
    const BuoyancyContact contact = BuoyancySystem::contact(level, getPosition(), buoyancy);
    if (contact.floats()) {
        if (buoyancy.mApplyGravity)
            motion.y -= PhysicsComponent::DEFAULT_GRAVITY;
        motion.y = BuoyancySystem::floatVelocity(motion.y, getPosition().y, getBuoyancyTimer(), buoyancy, contact,
                                                 getFlags().get(ActorFlag::Leashed));
    } else if (!result.mOnGround) {
        motion.y -= PhysicsComponent::DEFAULT_GRAVITY;
    }
    setMotion(motion);
    if (buoyancy.mEnabled)
        setBuoyancyTimer(BuoyancySystem::advanceTimer(getBuoyancyTimer(), buoyancy, motion, waveRandom()));
}

void BoatActor::fillSpawnMetadata(EntityDataMap &metadata) const {
    EntityDataEntry flags;
    flags.mId = ActorFlags::FLAGS_DATA_ID;
    flags.mFormat = EntityDataFormat::Long;
    flags.mLongValue = getFlags().getLowBits();
    metadata.mEntries.push_back(flags);

    EntityDataEntry flags2;
    flags2.mId = ActorFlags::FLAGS_2_DATA_ID;
    flags2.mFormat = EntityDataFormat::Long;
    flags2.mLongValue = getFlags().getHighBits();
    metadata.mEntries.push_back(flags2);

    EntityDataEntry flags3;
    flags3.mId = ActorFlags::FLAGS_3_DATA_ID;
    flags3.mFormat = EntityDataFormat::Long;
    flags3.mLongValue = getFlags().getExtendedBits();
    metadata.mEntries.push_back(flags3);

    mLeash.appendSpawnMetadata(metadata);

    EntityDataEntry variant;
    variant.mId = ACTOR_DATA_VARIANT;
    variant.mFormat = EntityDataFormat::Int;
    variant.mIntValue = mVariant;
    metadata.mEntries.push_back(variant);
}

Tag BoatActor::saveNbt() const {
    Tag data = ServerActor::saveNbt();
    data.putInt(TAG_VARIANT, mVariant);
    mLeash.saveNbt(data);
    if (mChest != nullptr)
        mChest->save(data);
    return data;
}

void BoatActor::loadNbt(const Tag &data) {
    ServerActor::loadNbt(data);
    mVariant = data.getInt(TAG_VARIANT, 0);
    mLeash.loadNbt(data);
    getFlags().set(ActorFlag::Leashed, mLeash.isLeashed());
}

void BoatActor::loadEquipment(const Tag &data, const PacketCodecContext &context) {
    if (mChest != nullptr)
        mChest->load(data, context);
}
