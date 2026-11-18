#include "World/Actor/Vehicle/MinecartActor.h"

#include "Core/Json/Json.h"
#include "Core/Math/AxisAlignedBB.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"
#include "Server/Commands/CommandMap.h"
#include "Server/Commands/CommandOrigin.h"
#include "Protocol/Types/ItemDefinition.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/DamageCause.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Movement/ActorCollisionSystem.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Actor/Movement/PhysicsComponent.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/RideSystem.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Level/Block/Actor/HopperBlockActor.h"
#include "World/Level/Block/BaseRailBlock.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/PoweredRailBlock.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
    struct Exit {
        int32_t x;
        int32_t y;
        int32_t z;
    };

    // by rail_direction, the two neighbours a rail joins
    const std::array<std::array<Exit, 2>, 10> EXITS = {{{{{0, 0, -1}, {0, 0, 1}}},
                                                        {{{-1, 0, 0}, {1, 0, 0}}},
                                                        {{{-1, -1, 0}, {1, 0, 0}}},
                                                        {{{-1, 0, 0}, {1, -1, 0}}},
                                                        {{{0, 0, -1}, {0, -1, 1}}},
                                                        {{{0, -1, -1}, {0, 0, 1}}},
                                                        {{{0, 0, 1}, {1, 0, 0}}},
                                                        {{{0, 0, 1}, {-1, 0, 0}}},
                                                        {{{0, 0, -1}, {-1, 0, 0}}},
                                                        {{{0, 0, -1}, {1, 0, 0}}}}};

    const char *const RIDEABLE = "minecraft:minecart";
    const char *const CHEST = "minecraft:chest_minecart";
    const char *const HOPPER = "minecraft:hopper_minecart";
    const char *const TNT = "minecraft:tnt_minecart";
    const char *const COMMAND_BLOCK = "minecraft:command_block_minecart";
    const char *const GOLDEN_RAIL = "minecraft:golden_rail";
    const char *const ACTIVATOR_RAIL = "minecraft:activator_rail";

    constexpr int32_t ACTOR_DATA_STRUCTURAL_INTEGRITY = 1;
    constexpr int32_t ACTOR_DATA_HURT_TIME = 11;
    constexpr int32_t ACTOR_DATA_HURT_DIRECTION = 12;
    constexpr int32_t ACTOR_DATA_DISPLAY_OFFSET = 17;
    constexpr int32_t ACTOR_DATA_COMMAND_BLOCK_ENABLED = 70;
    constexpr int32_t ACTOR_DATA_COMMAND_BLOCK_COMMAND = 71;
    constexpr int32_t ACTOR_DATA_COMMAND_BLOCK_LAST_OUTPUT = 72;
    constexpr int32_t ACTOR_DATA_COMMAND_BLOCK_TRACK_OUTPUT = 73;
    constexpr int32_t ACTOR_DATA_COMMAND_BLOCK_TICK_DELAY = 106;
    constexpr int32_t ACTOR_DATA_COMMAND_BLOCK_EXECUTE_ON_FIRST_TICK = 107;

    // minecraft:rail_movement max_speed
    const float MAX_SPEED = 0.4f;
    const float MAX_RAIL_SPEED = 2.0f;
    const float SLOPE_PUSH = 0.0078125f;
    const float BRAKE_STOP_SQUARED = 0.0009f;
    const float BRAKE = 0.5f;
    const float RIDDEN_SPEED = 0.75f;
    const float RIDER_PUSH = 0.1f;
    const float RIDER_PUSH_SPEED_SQUARED = 0.01f;
    const float HEIGHT_TO_SPEED = 0.05f;
    const float BOOST_MIN_SPEED = 0.01f;
    const float BOOST = 0.06f;
    const float START_PUSH = 0.02f;
    // the snapped height sits just above the rail so the move does not start inside the block
    const float SNAP_LIFT = 0.00001f;
    const float RIDDEN_SLOWDOWN = 0.997f;
    const float EMPTY_SLOWDOWN = 0.96f;
    const float CONTAINER_SLOWDOWN = 0.98f;
    const float CONTAINER_SLOWDOWN_PER_SIGNAL = 0.001f;
    const float OFF_RAIL_GROUND = 0.5f;
    const float OFF_RAIL_AIR = 0.95f;
    const float GRAVITY = PhysicsComponent::DEFAULT_GRAVITY;

    const float HURT_INTEGRITY = 8.0f;
    const int32_t HURT_TICKS = 10;
    const int32_t DISPLAY_OFFSET = 6;

    // minecraft:explode of the primed groups: power 3, a 4 second fuse
    const float TNT_POWER = 3.0f;
    const int32_t TNT_FUSE = 4 * 20;
    const int32_t HOPPER_COOLDOWN = 4;
    const int64_t COMMAND_PERIOD = 4;

    const float DEGREES_TO_RADIANS = 0.017453292f;
    // the pull in reach around the minecart
    const float PULL_IN_MARGIN = 0.2f;

    MinecartActor::Kind kindOf(const std::string &identifier) {
        if (identifier == CHEST)
            return MinecartActor::Kind::Chest;
        if (identifier == HOPPER)
            return MinecartActor::Kind::Hopper;
        if (identifier == TNT)
            return MinecartActor::Kind::Tnt;
        if (identifier == COMMAND_BLOCK)
            return MinecartActor::Kind::CommandBlock;
        return MinecartActor::Kind::Rideable;
    }

    // the block the minecart carries: dropped beside the minecart itself
    const char *carriedItem(MinecartActor::Kind kind) {
        switch (kind) {
            case MinecartActor::Kind::Chest:
                return "minecraft:chest";
            case MinecartActor::Kind::Hopper:
                return "minecraft:hopper";
            case MinecartActor::Kind::Tnt:
                return "minecraft:tnt";
            default:
                return nullptr;
        }
    }

    bool isRail(const BlockState *state) {
        return state != nullptr && BaseRailBlock::matches(state->mName);
    }

    int32_t directionOf(const BlockState &state) {
        return std::clamp(state.mStates.getInt("rail_direction", 0), 0, 9);
    }

    bool isAscending(int32_t direction) {
        return direction >= BaseRailBlock::ASCENDING_EAST && direction <= BaseRailBlock::ASCENDING_SOUTH;
    }

    // a full solid block a stopped minecart starts off from
    bool isSolidConductor(Level &level, int32_t x, int32_t y, int32_t z) {
        const BlockState *state = level.peekBlockPtr(x, y, z);
        return state != nullptr && BlockSupport::isSolid(*state) && !BaseRailBlock::matches(state->mName);
    }

    Vector3i railUnder(Level &level, const Vector3f &position) {
        Vector3i rail((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                      (int32_t) std::floor(position.z));
        if (!isRail(level.peekBlockPtr(rail.x, rail.y, rail.z)) && isRail(level.peekBlockPtr(rail.x, rail.y - 1, rail.z)))
            --rail.y;
        return rail;
    }

    float horizontalLength(const Vector3f &motion) {
        return std::sqrt(motion.x * motion.x + motion.z * motion.z);
    }

    // a command block minecart runs its command as itself, at the internal level of command blocks
    class MinecartCommandOrigin final : public CommandOrigin {
    public:
        MinecartCommandOrigin(const std::string &name, MinecartActor &minecart, Level &level)
                : mName(name), mMinecart(minecart), mLevel(level) {}

        std::string getSenderName() const override {
            return mName;
        }

        bool isPlayer() const override {
            return false;
        }

        ServerPlayer *asPlayer() override {
            return nullptr;
        }

        void sendMessage(const std::string &message) override {
            if (!mOutput.empty())
                mOutput += "\n";
            mOutput += message;
        }

        void sendTranslation(const std::string &key, const std::vector<std::string> &parameters) override {
            std::string message = key;
            for (const std::string &parameter: parameters)
                message += " " + parameter;
            sendMessage(message);
        }

        CommandPermission getCommandPermission() const override {
            return CommandPermission::Internal;
        }

        Vector3f getPosition() override {
            return mMinecart.getPosition();
        }

        Level *getLevel() override {
            return &mLevel;
        }

        Actor *getEntity() override {
            return &mMinecart;
        }

        const std::string &getOutput() const {
            return mOutput;
        }

    private:
        std::string mName;
        MinecartActor &mMinecart;
        Level &mLevel;
        std::string mOutput;
    };
}

FALCON_REGISTER_ACTOR(MinecartActor, RIDEABLE);

static const ActorClassRegistry::Registration gActorRegistrationChestMinecart(
        CHEST, [](uint64_t runtimeId, const std::string &identifier) -> std::unique_ptr<ServerActor> {
            return std::make_unique<MinecartActor>(runtimeId, identifier);
        });

static const ActorClassRegistry::Registration gActorRegistrationHopperMinecart(
        HOPPER, [](uint64_t runtimeId, const std::string &identifier) -> std::unique_ptr<ServerActor> {
            return std::make_unique<MinecartActor>(runtimeId, identifier);
        });

static const ActorClassRegistry::Registration gActorRegistrationTntMinecart(
        TNT, [](uint64_t runtimeId, const std::string &identifier) -> std::unique_ptr<ServerActor> {
            return std::make_unique<MinecartActor>(runtimeId, identifier);
        });

static const ActorClassRegistry::Registration gActorRegistrationCommandBlockMinecart(
        COMMAND_BLOCK, [](uint64_t runtimeId, const std::string &identifier) -> std::unique_ptr<ServerActor> {
            return std::make_unique<MinecartActor>(runtimeId, identifier);
        });

MinecartActor::MinecartActor(uint64_t runtimeId, const std::string &identifier)
        : ServerActor(runtimeId, identifier), mKind(kindOf(identifier)) {
    if (mKind == Kind::Chest || mKind == Kind::Hopper)
        mChest = std::make_unique<VehicleChest>(identifier);
}

bool MinecartActor::matches(const std::string &identifier) {
    return identifier == RIDEABLE || identifier == CHEST || identifier == HOPPER || identifier == TNT
           || identifier == COMMAND_BLOCK;
}

std::string MinecartActor::actorOfItem(const std::string &item) {
    if (item == "minecraft:minecart")
        return RIDEABLE;
    if (item == "minecraft:chest_minecart")
        return CHEST;
    if (item == "minecraft:hopper_minecart")
        return HOPPER;
    if (item == "minecraft:tnt_minecart")
        return TNT;
    if (item == "minecraft:command_block_minecart")
        return COMMAND_BLOCK;
    return std::string();
}

// minecraft:rideable seats [0, -0.2, 0]
Vector3f MinecartActor::getSeatOffset(size_t index, size_t passengerCount) const {
    (void) index;
    (void) passengerCount;
    return Vector3f(0.0f, -0.2f, 0.0f);
}

Container *MinecartActor::getEntityContainer() {
    return mChest == nullptr ? nullptr : &mChest->getContainer();
}

ServerPlayer *MinecartActor::_rider(ServerNetworkHandler &owner) const {
    if (getPassengers().empty())
        return nullptr;
    return dynamic_cast<ServerPlayer *>(RideSystem::resolve(owner, getPassengers().front()));
}

bool MinecartActor::onInteract(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (mExpired)
        return false;

    switch (mKind) {
        case Kind::Rideable:
            if (player.isRiding() || hasPassengers() || player.getFlags().get(ActorFlag::Sneaking))
                return false;
            return RideSystem::mount(owner, player, *this, true);
        case Kind::Chest:
        case Kind::Hopper:
            return mChest->open(owner, player, *this);
        case Kind::Tnt: {
            // minecraft:interact of the inactive group: flint and steel or a fire charge, while TNT explodes
            const ItemStack &held = player.getInventory().getItemInHand();
            const std::string item = held.mDefinition == nullptr ? std::string() : held.mDefinition->getIdentifier();
            if (mFuse >= 0 || (item != "minecraft:flint_and_steel" && item != "minecraft:fire_charge")
                || !owner.getLevelFor(*this).getGameRules().getBool("tntexplodes"))
                return false;
            owner.playLevelSound(owner.getLevelFor(*this), "ignite", getPosition(), getIdentifier());
            _prime(owner, TNT_FUSE);
            return true;
        }
        case Kind::CommandBlock:
            // the command block screen, as a block's, for operators in creative only
            if (!player.isOp() || !player.isCreative())
                return false;
            return player.getInventoryManager().openEntityContainer(*this, ContainerType::CommandBlock);
    }
    return false;
}

// the TNT minecart lights at once from fire, lava and a burning projectile, and goes off when an explosion breaks it
bool MinecartActor::senseDamage(ServerNetworkHandler &owner, float &amount, const ActorDamageSource &source) {
    (void) amount;
    mHitByExplosion = DamageCause::matches("entity_explosion", source)
                      || source.mDeathMessageKey.rfind("death.attack.explosion", 0) == 0;
    if (mKind != Kind::Tnt)
        return true;

    const bool burning = DamageCause::matches("fire", source) || DamageCause::matches("lava", source)
                         || (source.mProjectile && source.mDamager != nullptr
                             && source.mDamager->getFlags().get(ActorFlag::OnFire));
    if (burning && owner.getLevelFor(*this).getGameRules().getBool("tntexplodes")) {
        _explode(owner);
        return false;
    }
    return true;
}

bool MinecartActor::onHurt(ServerNetworkHandler &owner, float amount, ServerPlayer *source) {
    if (mExpired)
        return true;

    mHurtDirection = -mHurtDirection;
    mHurtTime = HURT_TICKS;
    mIntegrity = (int32_t) ((float) mIntegrity - amount * HURT_INTEGRITY);
    _sendHurt(owner);

    const bool creative = source != nullptr && source->isCreative();
    if (!creative && mIntegrity > 0)
        return true;
    // a TNT minecart broken while lit or by an explosion goes off instead of dropping
    if (mKind == Kind::Tnt && (mFuse >= 0 || mHitByExplosion)) {
        _explode(owner);
        return true;
    }
    _destroy(owner, !creative && owner.getLevelFor(*this).getGameRules().getBool("doentitydrops"));
    return true;
}

void MinecartActor::_sendHurt(ServerNetworkHandler &owner) {
    EntityDataMap metadata;
    for (const auto &[id, value]: {std::pair<int32_t, int32_t>{ACTOR_DATA_HURT_TIME, mHurtTime},
                                   {ACTOR_DATA_HURT_DIRECTION, mHurtDirection},
                                   {ACTOR_DATA_STRUCTURAL_INTEGRITY, mIntegrity}}) {
        EntityDataEntry entry;
        entry.mId = id;
        entry.mFormat = EntityDataFormat::Int;
        entry.mIntValue = value;
        metadata.mEntries.push_back(entry);
    }
    owner.sendActorMetadata(*this, metadata);
}

// the minecart and what it carries drop as separate items
void MinecartActor::_destroy(ServerNetworkHandler &owner, bool drop) {
    if (mExpired)
        return;
    mExpired = true;
    RideSystem::ejectAll(owner, *this);
    if (mChest != nullptr)
        mChest->dropAll(owner, *this);
    if (!drop)
        return;

    Level &level = owner.getLevelFor(*this);
    owner.spawnItemActor(level, "minecraft:minecart", 1, getPosition());
    if (const char *carried = carriedItem(mKind))
        owner.spawnItemActor(level, carried, 1, getPosition());
}

void MinecartActor::tick(ServerNetworkHandler &owner) {
    if (mExpired)
        return;
    if (mHurtTime > 0)
        --mHurtTime;

    ServerActor::tick(owner);
    if (mExpired)
        return;
    if (mKind == Kind::Rideable)
        _pullInMobs(owner);
    else if (mKind == Kind::Tnt)
        _tickTnt(owner);
    else if (mKind == Kind::Hopper)
        _tickHopper(owner);
}

void MinecartActor::tickMovement(ServerNetworkHandler &owner) {
    Level &level = owner.getLevelFor(*this);
    Vector3f motion = getMotion();
    motion.y -= GRAVITY;
    const Vector3f push = _push(owner);
    motion.x += push.x;
    motion.z += push.z;
    setMotion(motion);

    const Vector3i rail = railUnder(level, getPosition());
    const BlockState *state = level.peekBlockPtr(rail.x, rail.y, rail.z);
    if (isRail(state)) {
        const BlockState railState = *state;
        _moveAlongRail(owner, level, rail, railState);
        resetFallDistance();
        if (railState.mName == ACTIVATOR_RAIL)
            _activate(owner, railState.mStates.getByte(PoweredRailBlock::STATE_POWERED, 0) != 0);
        else
            _activate(owner, false);
    } else {
        _moveOffRail(level);
        _activate(owner, false);
    }

    RideSystem::syncPassengerPositions(owner, *this);
    if (needsMovementSync()) {
        owner.broadcastActorMove(*this);
        markMovementSynced();
    }
}

// pull_in_entities: a mob bumping an empty rideable minecart climbs in, unless a powered activator rail ejects it
void MinecartActor::_pullInMobs(ServerNetworkHandler &owner) {
    if (hasPassengers() || mActivated)
        return;

    const AxisAlignedBB box = ActorPushSystem::boundingBoxOf(*this);
    const AxisAlignedBB reach(box.mMinX - PULL_IN_MARGIN, box.mMinY, box.mMinZ - PULL_IN_MARGIN,
                              box.mMaxX + PULL_IN_MARGIN, box.mMaxY, box.mMaxZ + PULL_IN_MARGIN);
    Level &level = owner.getLevelFor(*this);
    MobActor *rider = nullptr;
    owner.forEachActorNear(getPosition(), WIDTH + PULL_IN_MARGIN, [&](ServerActor &actor) {
        MobActor *mob = dynamic_cast<MobActor *>(&actor);
        if (rider != nullptr || mob == nullptr || !mob->isAlive() || mob->isRiding() || mob->hasPassengers()
            || &owner.getLevelFor(*mob) != &level)
            return;
        if (ActorPushSystem::boundingBoxOf(*mob).intersectsWith(reach))
            rider = mob;
    });
    if (rider != nullptr)
        RideSystem::mount(owner, *rider, *this, false);
}

// the rail movement systems: the speed is laid along the rail, the minecart snapped onto it and moved, then
// adjusted to the height it reached
void MinecartActor::_moveAlongRail(ServerNetworkHandler &owner, Level &level, const Vector3i &rail,
                                   const BlockState &state) {
    const Vector3f start = getPosition();
    const std::optional<Vector3f> before = railPosition(level, start);
    const int32_t direction = directionOf(state);
    const std::array<Exit, 2> &exits = EXITS[(size_t) direction];

    bool powered = false;
    bool brake = false;
    if (state.mName == GOLDEN_RAIL) {
        powered = state.mStates.getByte(PoweredRailBlock::STATE_POWERED, 0) != 0;
        brake = !powered;
    }

    Vector3f motion = getMotion();
    switch (direction) {
        case BaseRailBlock::ASCENDING_EAST:
            motion.x -= SLOPE_PUSH;
            break;
        case BaseRailBlock::ASCENDING_WEST:
            motion.x += SLOPE_PUSH;
            break;
        case BaseRailBlock::ASCENDING_NORTH:
            motion.z += SLOPE_PUSH;
            break;
        case BaseRailBlock::ASCENDING_SOUTH:
            motion.z -= SLOPE_PUSH;
            break;
        default:
            break;
    }

    // the speed follows the rail, in the direction the minecart already goes
    float railX = (float) (exits[1].x - exits[0].x);
    float railZ = (float) (exits[1].z - exits[0].z);
    if (motion.x * railX + motion.z * railZ < 0.0f) {
        railX = -railX;
        railZ = -railZ;
    }
    const float speed = std::min(MAX_RAIL_SPEED, horizontalLength(motion));
    const float railLength = std::sqrt(railX * railX + railZ * railZ);
    motion.x = railX * speed / railLength;
    motion.z = railZ * speed / railLength;

    // a rider walking forward nudges a minecart that has nearly stopped
    if (ServerPlayer *rider = _rider(owner)) {
        if (mRiderForward > 0.0f && motion.x * motion.x + motion.z * motion.z < RIDER_PUSH_SPEED_SQUARED) {
            const float yaw = rider->getRotation().y * DEGREES_TO_RADIANS;
            motion.x -= std::sin(yaw) * RIDER_PUSH;
            motion.z += std::cos(yaw) * RIDER_PUSH;
            brake = false;
        }
    }

    if (brake) {
        if (motion.x * motion.x + motion.z * motion.z < BRAKE_STOP_SQUARED) {
            motion = Vector3f(0.0f, 0.0f, 0.0f);
        } else {
            motion.x *= BRAKE;
            motion.z *= BRAKE;
            motion.y = 0.0f;
        }
    }
    if (hasPassengers()) {
        motion.x *= RIDDEN_SPEED;
        motion.z *= RIDDEN_SPEED;
    }
    const LiquidContact liquid = LiquidBlocksFetch::at(level, start);
    const float maxSpeed = liquid.water ? MAX_SPEED * 0.5f : MAX_SPEED;
    motion.x = std::clamp(motion.x, -maxSpeed, maxSpeed);
    motion.z = std::clamp(motion.z, -maxSpeed, maxSpeed);
    setMotion(motion);

    // snapped onto the rail: the point of the segment closest to where the minecart stands
    const float fromX = (float) rail.x + 0.5f + (float) exits[0].x * 0.5f;
    const float fromZ = (float) rail.z + 0.5f + (float) exits[0].z * 0.5f;
    const float alongX = (float) rail.x + 0.5f + (float) exits[1].x * 0.5f - fromX;
    const float alongZ = (float) rail.z + 0.5f + (float) exits[1].z * 0.5f - fromZ;
    float t;
    if (alongX == 0.0f)
        t = start.z - (float) rail.z;
    else if (alongZ == 0.0f)
        t = start.x - (float) rail.x;
    else
        t = ((start.x - fromX) * alongX + (start.z - fromZ) * alongZ) * 2.0f;
    const float railY = (float) rail.y + (isAscending(direction) ? 1.0f : 0.0f) + SNAP_LIFT;
    setPosition(Vector3f(fromX + alongX * t, railY, fromZ + alongZ * t));

    _move(level, Vector3f(motion.x, 0.0f, motion.z));

    // a slope lifts the minecart a block when it reaches its upper end
    Vector3f position = getPosition();
    const int32_t blockX = (int32_t) std::floor(position.x);
    const int32_t blockZ = (int32_t) std::floor(position.z);
    if ((direction == BaseRailBlock::ASCENDING_SOUTH || direction == BaseRailBlock::ASCENDING_EAST)
        && blockX - rail.x == exits[0].x && blockZ - rail.z == exits[0].z)
        position.y += (float) exits[0].y;
    else if ((direction == BaseRailBlock::ASCENDING_WEST || direction == BaseRailBlock::ASCENDING_NORTH)
             && blockX - rail.x == exits[1].x && blockZ - rail.z == exits[1].z)
        position.y += (float) exits[1].y;
    setPosition(position);

    // going down gains speed, going up loses it
    motion = getMotion();
    const std::optional<Vector3f> after = railPosition(level, position);
    if (before.has_value() && after.has_value()) {
        const float horizontal = horizontalLength(motion);
        if (horizontal > 0.0f) {
            const float faster = (before->y - after->y) * HEIGHT_TO_SPEED + horizontal;
            motion.x = motion.x / horizontal * faster;
            motion.z = motion.z / horizontal * faster;
        }
        position.y = after->y;
        setPosition(position);
    }

    // leaving the rail's block turns the speed toward the block entered
    const int32_t enteredX = (int32_t) std::floor(position.x) - rail.x;
    const int32_t enteredZ = (int32_t) std::floor(position.z) - rail.z;
    if (enteredX != 0 || enteredZ != 0) {
        const float horizontal = horizontalLength(motion);
        motion.x = (float) enteredX * horizontal;
        motion.z = (float) enteredZ * horizontal;
    }
    setMotion(motion);
    _slowDown();

    if (!powered || brake)
        return;
    motion = getMotion();
    const float horizontal = horizontalLength(motion);
    if (horizontal > BOOST_MIN_SPEED) {
        motion.x += motion.x / horizontal * BOOST;
        motion.z += motion.z / horizontal * BOOST;
    } else if (direction == BaseRailBlock::STRAIGHT_EAST_WEST) {
        // a stopped minecart starts away from a solid block beside the rail
        if (isSolidConductor(level, rail.x - 1, rail.y, rail.z))
            motion.x = START_PUSH;
        else if (isSolidConductor(level, rail.x + 1, rail.y, rail.z))
            motion.x = -START_PUSH;
    } else if (direction == BaseRailBlock::STRAIGHT_NORTH_SOUTH) {
        if (isSolidConductor(level, rail.x, rail.y, rail.z - 1))
            motion.z = START_PUSH;
        else if (isSolidConductor(level, rail.x, rail.y, rail.z + 1))
            motion.z = -START_PUSH;
    }
    setMotion(motion);
}

// off a rail the minecart keeps its top speed, drags on the ground and a little in the air
void MinecartActor::_moveOffRail(Level &level) {
    Vector3f motion = getMotion();
    motion.x = std::clamp(motion.x, -MAX_SPEED, MAX_SPEED);
    motion.z = std::clamp(motion.z, -MAX_SPEED, MAX_SPEED);
    if (isOnGround()) {
        motion.x *= OFF_RAIL_GROUND;
        motion.y *= OFF_RAIL_GROUND;
        motion.z *= OFF_RAIL_GROUND;
    }
    setMotion(motion);
    _move(level, motion);
    if (!isOnGround()) {
        motion = getMotion();
        motion.x *= OFF_RAIL_AIR;
        motion.y *= OFF_RAIL_AIR;
        motion.z *= OFF_RAIL_AIR;
        setMotion(motion);
    }
}

// applyNaturalSlowdown: a carried container slows with its fill, a ridden minecart barely; the vertical speed stops
void MinecartActor::_slowDown() {
    float factor = hasPassengers() ? RIDDEN_SLOWDOWN : EMPTY_SLOWDOWN;
    if (mChest != nullptr) {
        const Container &container = mChest->getContainer();
        factor = (float) (15 - Block::containerSignal(container)) * CONTAINER_SLOWDOWN_PER_SIGNAL
                 + CONTAINER_SLOWDOWN;
    }
    const Vector3f motion = getMotion();
    setMotion(Vector3f(motion.x * factor, 0.0f, motion.z * factor));
}

void MinecartActor::_move(Level &level, const Vector3f &motion) {
    AxisAlignedBB box = ActorPushSystem::boundingBoxOf(*this);
    const ActorMoveResult result = ActorCollisionSystem::move(level, box, motion, 0.0f, isOnGround(), this);
    setPosition(Vector3f((box.mMinX + box.mMaxX) * 0.5f, box.mMinY, (box.mMinZ + box.mMaxZ) * 0.5f));
    Vector3f after = getMotion();
    if (result.mCollidedX)
        after.x = 0.0f;
    if (result.mCollidedY)
        after.y = 0.0f;
    if (result.mCollidedZ)
        after.z = 0.0f;
    setMotion(after);
    setOnGround(result.mOnGround);
}

// legacy_minecart: pushed like a boat, but two minecarts on one line share their speed instead
Vector3f MinecartActor::_push(ServerNetworkHandler &owner) {
    const json::Value *definition = EntityDefinitions::find(getTypeId());
    const json::Value *components = definition == nullptr ? nullptr : definition->get("components");
    mPushPresets.refresh(components == nullptr ? nullptr : components->get("minecraft:pushable_by_entity"));
    Vector3f total(0.0f, 0.0f, 0.0f);
    if (mPushPresets.mPresets.empty())
        return total;

    const AxisAlignedBB self = ActorPushSystem::boundingBoxOf(*this);
    const auto pushBy = [&](Actor &pusher, float pusherWidth) {
        if (pusher.getVehicleId() == getUniqueId() || pusher.getUniqueId() == getVehicleId()
            || !ActorPushSystem::boundingBoxOf(pusher).intersectsWith(self))
            return;
        const PushableByEntityPreset *preset = PushableByEntity::findPreset(owner, *this, mPushPresets, pusher);
        if (preset == nullptr || preset->mMode != PushMode::LegacyMinecart)
            return;
        const float offsetX = pusher.getPosition().x - getPosition().x;
        const float offsetZ = pusher.getPosition().z - getPosition().z;
        if (!PushableByEntity::inReach(*preset, offsetX, offsetZ, pusherWidth))
            return;

        const Vector3f push = PushableByEntity::legacyBoatPush(*preset, offsetX, offsetZ);
        ServerActor *other = dynamic_cast<ServerActor *>(&pusher);
        if (other == nullptr || !other->isMinecart()) {
            total = Vector3f(total.x + push.x, 0.0f, total.z + push.z);
            if (other == nullptr)
                return;
            const Vector3f motion = other->getMotion();
            other->setMotion(Vector3f(motion.x - push.x * preset->mPushScaleOther, motion.y,
                                      motion.z - push.z * preset->mPushScaleOther));
            return;
        }

        // only a minecart ahead or behind on the line counts
        const float length = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
        const float yaw = getRotation().y * DEGREES_TO_RADIANS;
        if (length < 0.0001f || std::fabs((offsetX * std::cos(yaw) + offsetZ * std::sin(yaw)) / length) < 0.8f)
            return;
        const Vector3f mine = getMotion();
        const Vector3f theirs = other->getMotion();
        const float shareX = (mine.x + theirs.x) * 0.5f;
        const float shareZ = (mine.z + theirs.z) * 0.5f;
        setMotion(Vector3f(mine.x * 0.2f, mine.y, mine.z * 0.2f));
        other->setMotion(Vector3f(theirs.x * 0.2f + shareX - push.x, theirs.y, theirs.z * 0.2f + shareZ - push.z));
        total = Vector3f(total.x + shareX + push.x, 0.0f, total.z + shareZ + push.z);
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

// minecraft:rail_sensor: what a powered activator rail under the minecart does
void MinecartActor::_activate(ServerNetworkHandler &owner, bool powered) {
    const bool changed = powered != mActivated;
    mActivated = powered;
    switch (mKind) {
        case Kind::Rideable:
            if (powered)
                RideSystem::ejectAll(owner, *this);
            break;
        case Kind::Tnt:
            if (powered && changed && mFuse < 0 && owner.getLevelFor(*this).getGameRules().getBool("tntexplodes"))
                _prime(owner, TNT_FUSE);
            break;
        case Kind::Hopper:
            if (changed)
                mHopperEnabled = !powered;
            break;
        case Kind::CommandBlock:
            if (powered)
                _runCommand(owner);
            break;
        case Kind::Chest:
            break;
    }
}

void MinecartActor::_prime(ServerNetworkHandler &owner, int32_t fuse) {
    mFuse = fuse;
    getFlags().set(ActorFlag::Ignited, true);
    owner.syncActorFlags(*this);
}

void MinecartActor::_tickTnt(ServerNetworkHandler &owner) {
    // standing in lava or fire burns the minecart, which lights its TNT at once
    Level &level = owner.getLevelFor(*this);
    const Vector3f position = getPosition();
    const BlockState *inside = level.peekBlockPtr((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                                                  (int32_t) std::floor(position.z));
    if (inside != nullptr && level.getGameRules().getBool("tntexplodes")
        && (LiquidBlocksFetch::at(level, position).lava || inside->mName == "minecraft:fire"
            || inside->mName == "minecraft:soul_fire")) {
        _explode(owner);
        return;
    }
    if (mFuse < 0)
        return;
    if (mFuse == 0) {
        _explode(owner);
        return;
    }
    --mFuse;
}

void MinecartActor::_explode(ServerNetworkHandler &owner) {
    if (mExpired)
        return;
    mExpired = true;
    RideSystem::ejectAll(owner, *this);
    owner.explodeAfterActors(owner.getLevelFor(*this), getPosition(), TNT_POWER, *this, true);
}

// minecraft:item_hopper: the hopper minecart takes from the container above it and picks up items around it
void MinecartActor::_tickHopper(ServerNetworkHandler &owner) {
    if (!mHopperEnabled || mChest == nullptr)
        return;
    if (mHopperCooldown > 0) {
        --mHopperCooldown;
        return;
    }

    Level &level = owner.getLevelFor(*this);
    Container &container = mChest->getContainer();
    const Vector3f position = getPosition();
    const Vector3i above((int32_t) std::floor(position.x), (int32_t) std::floor(position.y) + 1,
                         (int32_t) std::floor(position.z));
    const float half = WIDTH * 0.5f;
    if (HopperBlockActor::pullFrom(owner, level, above, container, above)
        || HopperBlockActor::pickUpItems(owner, level, position.x - half - 0.25f, position.y,
                                         position.z - half - 0.25f, position.x + half + 0.25f,
                                         position.y + HEIGHT + 0.5f, position.z + half + 0.25f, container))
        mHopperCooldown = HOPPER_COOLDOWN;
}

void MinecartActor::setCommand(ServerNetworkHandler &owner, const std::string &command, const std::string &name,
                               bool trackOutput) {
    mCommand = command;
    mCommandName = name;
    mTrackOutput = trackOutput;
    if (!trackOutput)
        mLastOutput.clear();
    _sendCommand(owner);
}

void MinecartActor::_sendCommand(ServerNetworkHandler &owner) {
    EntityDataMap metadata;
    _appendCommand(metadata);
    owner.sendActorMetadata(*this, metadata);
}

// the command block data the minecart's screen reads
void MinecartActor::_appendCommand(EntityDataMap &metadata) const {
    EntityDataEntry enabled;
    enabled.mId = ACTOR_DATA_COMMAND_BLOCK_ENABLED;
    enabled.mFormat = EntityDataFormat::Byte;
    enabled.mByteValue = 1;
    metadata.mEntries.push_back(enabled);

    EntityDataEntry command;
    command.mId = ACTOR_DATA_COMMAND_BLOCK_COMMAND;
    command.mFormat = EntityDataFormat::String;
    command.mStringValue = mCommand;
    metadata.mEntries.push_back(command);

    EntityDataEntry output;
    output.mId = ACTOR_DATA_COMMAND_BLOCK_LAST_OUTPUT;
    output.mFormat = EntityDataFormat::String;
    output.mStringValue = mLastOutput;
    metadata.mEntries.push_back(output);

    EntityDataEntry track;
    track.mId = ACTOR_DATA_COMMAND_BLOCK_TRACK_OUTPUT;
    track.mFormat = EntityDataFormat::Byte;
    track.mByteValue = mTrackOutput ? 1 : 0;
    metadata.mEntries.push_back(track);

    EntityDataEntry delay;
    delay.mId = ACTOR_DATA_COMMAND_BLOCK_TICK_DELAY;
    delay.mFormat = EntityDataFormat::Int;
    delay.mIntValue = 0;
    metadata.mEntries.push_back(delay);

    EntityDataEntry firstTick;
    firstTick.mId = ACTOR_DATA_COMMAND_BLOCK_EXECUTE_ON_FIRST_TICK;
    firstTick.mFormat = EntityDataFormat::Byte;
    firstTick.mByteValue = 0;
    metadata.mEntries.push_back(firstTick);
}

void MinecartActor::_runCommand(ServerNetworkHandler &owner) {
    const int64_t tick = owner.getCurrentTick();
    if (mCommand.empty() || (mLastCommandTick >= 0 && tick - mLastCommandTick < COMMAND_PERIOD))
        return;
    mLastCommandTick = tick;
    if (!owner.getLevelFor(*this).getGameRules().getBool("commandblocksenabled"))
        return;
    MinecartCommandOrigin origin(mCommandName.empty() ? std::string("@") : mCommandName, *this,
                                 owner.getLevelFor(*this));
    owner.getCommands().dispatch(origin, mCommand);
    const std::string previous = mLastOutput;
    mLastOutput = mTrackOutput && owner.getLevelFor(*this).getGameRules().getBool("commandblockoutput")
                          ? origin.getOutput()
                          : std::string();
    if (mLastOutput != previous)
        _sendCommand(owner);
}

void MinecartActor::readRider(ServerNetworkHandler &owner, ServerPlayer &player, const PlayerAuthInputPacket &packet) {
    MinecartActor *minecart = dynamic_cast<MinecartActor *>(RideSystem::resolve(owner, player.getVehicleId()));
    if (minecart != nullptr && minecart->_rider(owner) == &player)
        minecart->mRiderForward = std::isfinite(packet.mMotionY) ? packet.mMotionY : 0.0f;
}

std::optional<Vector3f> MinecartActor::railPosition(Level &level, const Vector3f &position) {
    const Vector3i rail = railUnder(level, position);
    const BlockState *state = level.peekBlockPtr(rail.x, rail.y, rail.z);
    if (!isRail(state))
        return std::nullopt;

    const std::array<Exit, 2> &exits = EXITS[(size_t) directionOf(*state)];
    const float fromX = (float) rail.x + 0.5f + (float) exits[0].x * 0.5f;
    const float fromY = (float) rail.y + (float) exits[0].y * 0.5f;
    const float fromZ = (float) rail.z + 0.5f + (float) exits[0].z * 0.5f;
    const float alongX = (float) rail.x + 0.5f + (float) exits[1].x * 0.5f - fromX;
    const float alongY = ((float) rail.y + (float) exits[1].y * 0.5f - fromY) * 2.0f;
    const float alongZ = (float) rail.z + 0.5f + (float) exits[1].z * 0.5f - fromZ;
    float t;
    if (alongX == 0.0f)
        t = position.z - (float) rail.z;
    else if (alongZ == 0.0f)
        t = position.x - (float) rail.x;
    else
        t = ((position.x - fromX) * alongX + (position.z - fromZ) * alongZ) * 2.0f;

    Vector3f result(fromX + alongX * t, fromY + alongY * t, fromZ + alongZ * t);
    if (alongY < 0.0f)
        result.y += 1.0f;
    else if (alongY > 0.0f)
        result.y += 0.5f;
    return result;
}

void MinecartActor::fillSpawnMetadata(EntityDataMap &metadata) const {
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

    EntityDataEntry offset;
    offset.mId = ACTOR_DATA_DISPLAY_OFFSET;
    offset.mFormat = EntityDataFormat::Int;
    offset.mIntValue = DISPLAY_OFFSET;
    metadata.mEntries.push_back(offset);

    if (mKind == Kind::CommandBlock)
        _appendCommand(metadata);
}

Tag MinecartActor::saveNbt() const {
    Tag data = ServerActor::saveNbt();
    if (mChest != nullptr)
        mChest->save(data);
    if (mKind == Kind::Tnt && mFuse >= 0)
        data.putInt("TNTFuse", mFuse);
    if (mKind == Kind::CommandBlock) {
        data.putString("Command", mCommand);
        data.putString("CustomName", mCommandName);
        data.putString("LastOutput", mLastOutput);
        data.putByte("TrackOutput", mTrackOutput ? 1 : 0);
    }
    return data;
}

void MinecartActor::loadNbt(const Tag &data) {
    ServerActor::loadNbt(data);
    mFuse = data.getInt("TNTFuse", -1);
    getFlags().set(ActorFlag::Ignited, mFuse >= 0);
    mCommand = data.getString("Command", std::string());
    mCommandName = data.getString("CustomName", std::string());
    mLastOutput = data.getString("LastOutput", std::string());
    mTrackOutput = data.getByte("TrackOutput", 1) != 0;
}

void MinecartActor::loadEquipment(const Tag &data, const PacketCodecContext &context) {
    if (mChest != nullptr)
        mChest->load(data, context);
}
