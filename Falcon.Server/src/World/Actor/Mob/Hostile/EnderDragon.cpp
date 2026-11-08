#include "World/Actor/Mob/Hostile/EnderDragon.h"

#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/BossEventPacket.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Definition/EntityEvents.h"
#include "World/Actor/Misc/EndCrystalActor.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Dimension/EndDragonFight.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <cmath>
#include <limits>

FALCON_REGISTER_ACTOR(EnderDragon, EnderDragon::IDENTIFIER);

namespace {

    const float DEGREES_TO_RADIANS = 0.017453292f;
    const float RADIANS_TO_DEGREES = 57.295776f;

    const float BOSS_HUD_RANGE = 125.0f;
    const int32_t BOSS_ACTION_CREATE = 0;
    const int32_t BOSS_ACTION_REMOVE = 2;
    const int32_t BOSS_ACTION_UPDATE_PERCENT = 4;
    const int32_t BOSS_COLOR_PURPLE = 5;

    const float CRYSTAL_SEARCH_RANGE = 32.0f;
    const int32_t CRYSTAL_HEAL_INTERVAL = 10;
    const float CRYSTAL_DESTROYED_DAMAGE = 10.0f;
    const float CRYSTAL_ATTACKER_RANGE = 64.0f;

    const float WING_DAMAGE = 5.0f;
    const float HEAD_DAMAGE = 10.0f;
    const float WING_PUSH = 4.0f;
    const float WING_LIFT = 0.2f;

    const int32_t DESTROY_PARTICLE_EVENT = 2009;
    const int32_t DESTROY_PARTICLE_DATA = 65;

    const int32_t NODE_LOAD_RADIUS = 60;
    const int32_t NODE_HEIGHT_LIMIT = 128;
    const int32_t END_SEA_LEVEL = 32;

    const float DEATH_RISE = 0.1f;
    const int32_t DEATH_SOUND_TICK = 2;

    const char *const FIREBALL_IDENTIFIER = "minecraft:dragon_fireball";
    const float FIREBALL_SPEED = 1.3f;

    const char *const ON_DEATH_TRIGGER = "minecraft:on_death";
    const char *const ON_TAKEOFF_TRIGGER = "minecraft:on_start_takeoff";

    // sub_14ACCE300: each node lists its neighbours as a bit mask over the 24 nodes
    const uint32_t NODE_ADJACENCY[EnderDragon::NODE_COUNT] = {
            6146, 8197, 8202, 16404, 32808, 32848, 65696, 131392, 131712, 263424, 526848, 525313,
            1581057, 3166214, 2138120, 6373424, 4358208, 12910976, 9044480, 9706496, 15216640, 13688832,
            11763712, 8257536};

    // the half sizes of the head, neck, body, three tail parts and two wings (sub_14ACC9AC0)
    const float PART_SIZES[(size_t) EnderDragon::Part::Count][2] = {
            {1.0f, 1.0f}, {3.0f, 3.0f}, {5.0f, 3.0f}, {2.0f, 2.0f}, {2.0f, 2.0f}, {2.0f, 2.0f}, {4.0f, 3.0f},
            {4.0f, 3.0f}};

    // blocks the dragon cannot fly through (sub_14ACCCD70)
    const char *const IMMUNE_BLOCKS[] = {
            "minecraft:bedrock", "minecraft:obsidian", "minecraft:crying_obsidian", "minecraft:respawn_anchor",
            "minecraft:end_stone", "minecraft:end_portal_frame", "minecraft:end_portal", "minecraft:end_gateway",
            "minecraft:iron_bars", "minecraft:reinforced_deepslate", "minecraft:barrier", "minecraft:allow",
            "minecraft:deny", "minecraft:border_block", "minecraft:command_block",
            "minecraft:repeating_command_block", "minecraft:chain_command_block", "minecraft:structure_block",
            "minecraft:jigsaw"};

    float wrapDegrees(float degrees) {
        float wrapped = std::fmod(degrees + 180.0f, 360.0f);
        if (wrapped < 0.0f)
            wrapped += 360.0f;
        return wrapped - 180.0f;
    }

    float clampf(float value, float minimum, float maximum) {
        return std::max(minimum, std::min(maximum, value));
    }

    bool isImmune(const std::string &name) {
        for (const char *immune: IMMUNE_BLOCKS) {
            if (name == immune)
                return true;
        }

        return false;
    }

    bool passesThrough(const std::string &name) {
        return name == "minecraft:air" || name == "minecraft:structure_void" || name == "minecraft:fire"
               || name == "minecraft:soul_fire";
    }

    float distance(const Vector3i &left, const Vector3i &right) {
        const float dx = (float) right.x - (float) left.x;
        const float dy = (float) right.y - (float) left.y;
        const float dz = (float) right.z - (float) left.z;
        return std::sqrt(dz * dz + (dy * dy + dx * dx));
    }

    // the A* state of one flight node
    struct SearchNode {
        int32_t mHeapIndex = -1;
        float mCost = 0.0f;
        float mHeuristic = 0.0f;
        float mTotal = 0.0f;
        int32_t mCameFrom = -1;
        bool mClosed = false;
    };

    // the binary heap the game orders open nodes with, ties included
    class NodeHeap {
    public:
        explicit NodeHeap(std::vector<SearchNode> &nodes) : mNodes(nodes) {
        }

        bool isEmpty() const {
            return mHeap.empty();
        }

        void insert(int32_t node) {
            mHeap.push_back(node);
            mNodes[(size_t) node].mHeapIndex = (int32_t) mHeap.size() - 1;
            _up((int32_t) mHeap.size() - 1);
        }

        int32_t pop() {
            const int32_t top = mHeap[0];
            mHeap[0] = mHeap.back();
            mHeap.pop_back();
            if (!mHeap.empty())
                _down(0);
            mNodes[(size_t) top].mHeapIndex = -1;
            return top;
        }

        void changeCost(int32_t node, float total) {
            SearchNode &search = mNodes[(size_t) node];
            const float previous = search.mTotal;
            search.mTotal = total;
            if (total < previous)
                _up(search.mHeapIndex);
            else
                _down(search.mHeapIndex);
        }

    private:
        void _place(int32_t index, int32_t node) {
            mHeap[(size_t) index] = node;
            mNodes[(size_t) node].mHeapIndex = index;
        }

        void _up(int32_t index) {
            const int32_t node = mHeap[(size_t) index];
            const float total = mNodes[(size_t) node].mTotal;
            while (index > 0) {
                const int32_t parent = (index - 1) >> 1;
                const int32_t parentNode = mHeap[(size_t) parent];
                if (total >= mNodes[(size_t) parentNode].mTotal)
                    break;

                _place(index, parentNode);
                index = parent;
            }
            _place(index, node);
        }

        void _down(int32_t index) {
            const int32_t node = mHeap[(size_t) index];
            const float total = mNodes[(size_t) node].mTotal;
            const int32_t size = (int32_t) mHeap.size();
            while (true) {
                const int32_t left = 1 + (index << 1);
                const int32_t right = left + 1;
                if (left >= size)
                    break;

                const int32_t leftNode = mHeap[(size_t) left];
                const float leftTotal = mNodes[(size_t) leftNode].mTotal;
                const float rightTotal = right >= size ? std::numeric_limits<float>::max()
                                                       : mNodes[(size_t) mHeap[(size_t) right]].mTotal;
                if (rightTotal > leftTotal) {
                    if (total <= leftTotal)
                        break;

                    _place(index, leftNode);
                    index = left;
                } else {
                    if (right >= size || total <= rightTotal)
                        break;

                    _place(index, mHeap[(size_t) right]);
                    index = right;
                }
            }
            _place(index, node);
        }

        std::vector<SearchNode> &mNodes;
        std::vector<int32_t> mHeap;
    };

}

EnderDragon::EnderDragon(uint64_t runtimeId, const std::string &identifier)
        : HostileActor(runtimeId, identifier), mRandom((uint32_t) runtimeId) {
}

float EnderDragon::nextFloat() {
    return std::uniform_real_distribution<float>(0.0f, 1.0f)(mRandom);
}

int32_t EnderDragon::nextInt(int32_t bound) {
    if (bound <= 0)
        return 0;

    return (int32_t) (mRandom() % (uint32_t) bound);
}

bool EnderDragon::isSitting() const {
    return getFlags().get(ActorFlag::Sitting);
}

void EnderDragon::setSitting(ServerNetworkHandler &owner, bool sitting) {
    if (isSitting() == sitting)
        return;

    getFlags().set(ActorFlag::Sitting, sitting);
    owner.syncActorFlags(*this);
}

void EnderDragon::fireTrigger(ServerNetworkHandler &owner, const char *component) {
    EntityEvents::fireTrigger(owner, *this, getComponent(component));
}

Vector3f EnderDragon::getPartCenter(Part part) const {
    const AxisAlignedBB &box = mParts[(size_t) part];
    return Vector3f((box.mMinX + box.mMaxX) * 0.5f, (box.mMinY + box.mMaxY) * 0.5f, (box.mMinZ + box.mMaxZ) * 0.5f);
}

int32_t EnderDragon::getPortalY(Level &level) {
    for (int32_t y = level.getMaxY(); y >= level.getMinY(); --y) {
        const BlockState *state = level.peekBlockPtr(0, y, 0);
        if (state != nullptr && state->mName != "minecraft:air")
            return y + 1;
    }

    return level.getMinY();
}

ServerPlayer *EnderDragon::nearestPlayer(ServerNetworkHandler &owner, const Level &level, const Vector3f &position,
                                         float range) {
    ServerPlayer *nearest = nullptr;
    float best = range * range;
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || !player.isIn(level))
            continue;

        const Vector3f other = player.getPosition();
        const float dx = other.x - position.x;
        const float dy = other.y - position.y;
        const float dz = other.z - position.z;
        const float distanceSquared = dx * dx + dy * dy + dz * dz;
        if (distanceSquared < best) {
            best = distanceSquared;
            nearest = &player;
        }
    }

    return nearest;
}

uint32_t EnderDragon::getNodeAdjacency(int32_t node) {
    if (node < 0 || node >= NODE_COUNT)
        return 0;

    return NODE_ADJACENCY[node];
}

void EnderDragon::buildNodes(const std::function<int32_t(int32_t x, int32_t z)> &surfaceAt, int32_t seaLevel) {
    mNodes.clear();
    mNodes.reserve(NODE_COUNT);

    for (int32_t i = 0; i < NODE_COUNT; i++) {
        int32_t lift = 5;
        int32_t step = i;
        float angle = 0.2617994f;
        float radius = 60.0f;
        if (i >= 20) {
            step = i - 20;
            angle = 0.7853982f;
            radius = 20.0f;
        } else if (i >= 12) {
            step = i - 12;
            lift = 15;
            angle = 0.3926991f;
            radius = 40.0f;
        }

        const float turn = (float) step * angle + -3.1415927f;
        const float doubled = turn + turn;
        const int32_t x = (int32_t) (std::cos(doubled) * radius);
        const int32_t z = (int32_t) (std::sin(doubled) * radius);
        const int32_t y = std::min(std::max(seaLevel + 10, surfaceAt(x, z) + lift), NODE_HEIGHT_LIMIT);
        mNodes.emplace_back(x, y, z);
    }
}

bool EnderDragon::_ensureNodes(ServerNetworkHandler &owner) {
    if (!mNodes.empty())
        return true;

    Level &level = owner.getLevelFor(*this);
    for (int32_t chunkX = -NODE_LOAD_RADIUS >> 4; chunkX <= NODE_LOAD_RADIUS >> 4; chunkX++) {
        for (int32_t chunkZ = -NODE_LOAD_RADIUS >> 4; chunkZ <= NODE_LOAD_RADIUS >> 4; chunkZ++) {
            if (!level.isChunkPopulated(chunkX, chunkZ))
                return false;
        }
    }

    buildNodes([&level](int32_t x, int32_t z) {
        for (int32_t y = level.getMaxY(); y >= level.getMinY(); --y) {
            const BlockState *state = level.peekBlockPtr(x, y, z);
            if (state != nullptr && state->mName != "minecraft:air")
                return y + 1;
        }
        return level.getMinY();
    }, END_SEA_LEVEL);
    return true;
}

int32_t EnderDragon::findClosestNode(ServerNetworkHandler &owner) {
    const Vector3f position = getPosition();
    return findClosestNode(owner, position.x, position.y, position.z);
}

int32_t EnderDragon::findClosestNode(ServerNetworkHandler &owner, float x, float y, float z) {
    if (!_ensureNodes(owner))
        return 0;

    const Vector3i from((int32_t) std::floor(x), (int32_t) std::floor(y), (int32_t) std::floor(z));
    float best = 10000.0f;
    int32_t closest = 0;
    for (int32_t i = mCrystalsAlive == 0 ? 12 : 0; i < (int32_t) mNodes.size(); i++) {
        const Vector3i &node = mNodes[(size_t) i];
        const int64_t dx = (int64_t) node.x - from.x;
        const int64_t dy = (int64_t) node.y - from.y;
        const int64_t dz = (int64_t) node.z - from.z;
        const float distanceSquared = (float) (dx * dx + dy * dy + dz * dz);
        if (best > distanceSquared)
            closest = i;
        best = std::min(distanceSquared, best);
    }

    return closest;
}

std::unique_ptr<EnderDragon::Path> EnderDragon::findPath(int32_t from, int32_t to,
                                                         const std::optional<Vector3i> &finalNode) {
    const int32_t count = (int32_t) mNodes.size();
    if (from < 0 || from >= count || to < 0 || to >= count)
        return nullptr;

    std::vector<SearchNode> search((size_t) count);
    NodeHeap open(search);

    SearchNode &start = search[(size_t) from];
    start.mCost = 0.0f;
    start.mHeuristic = distance(mNodes[(size_t) from], mNodes[(size_t) to]);
    start.mTotal = start.mHeuristic;
    open.insert(from);

    const int32_t firstNode = mCrystalsAlive == 0 ? 12 : 0;
    int32_t closest = from;
    int32_t end = -1;

    while (!open.isEmpty()) {
        const int32_t node = open.pop();
        if (node == to) {
            end = to;
            break;
        }

        if (distance(mNodes[(size_t) closest], mNodes[(size_t) to]) > distance(mNodes[(size_t) node],
                                                                                   mNodes[(size_t) to]))
            closest = node;

        search[(size_t) node].mClosed = true;
        for (int32_t next = firstNode; next < count; next++) {
            if ((NODE_ADJACENCY[node] & (1u << next)) == 0)
                continue;

            SearchNode &neighbour = search[(size_t) next];
            if (neighbour.mClosed)
                continue;

            const float cost = search[(size_t) node].mCost + distance(mNodes[(size_t) node], mNodes[(size_t) next]);
            if (neighbour.mHeapIndex >= 0 && neighbour.mCost <= cost)
                continue;

            neighbour.mCameFrom = node;
            neighbour.mCost = cost;
            neighbour.mHeuristic = distance(mNodes[(size_t) next], mNodes[(size_t) to]);
            if (neighbour.mHeapIndex >= 0) {
                open.changeCost(next, neighbour.mCost + neighbour.mHeuristic);
            } else {
                neighbour.mTotal = neighbour.mCost + neighbour.mHeuristic;
                open.insert(next);
            }
        }
    }

    if (end < 0) {
        if (closest == from)
            return nullptr;
        end = closest;
    }

    std::vector<Vector3i> nodes;
    if (finalNode.has_value())
        nodes.push_back(*finalNode);
    for (int32_t node = end; node >= 0; node = search[(size_t) node].mCameFrom)
        nodes.push_back(mNodes[(size_t) node]);
    std::reverse(nodes.begin(), nodes.end());
    return std::make_unique<Path>(std::move(nodes));
}

void EnderDragon::tick(ServerNetworkHandler &owner) {
    HostileActor::tick(owner);
    _tickBossBar(owner);
}

EnderDragon::LatencyPosition EnderDragon::_getLatencyPosition(int32_t offset) const {
    // sub_14ACCC560 reads the slot before the offset one, its yaw kept next to the newer slot's
    const int32_t newer = (mPositionPointer - offset) & 63;
    const int32_t older = (mPositionPointer - offset - 1) & 63;
    const LatencyPosition &base = mPositions[(size_t) newer];
    const LatencyPosition &target = mPositions[(size_t) older];
    return LatencyPosition{base.mYaw + wrapDegrees(target.mYaw - base.mYaw), target.mY};
}

void EnderDragon::_tickSounds(ServerNetworkHandler &owner, Level &level) {
    if (getFlags().get(ActorFlag::Silent))
        return;

    const float before = std::cos((mPreviousFlapTime + mPreviousFlapTime) * 3.1415927f);
    const float after = std::cos((mFlapTime + mFlapTime) * 3.1415927f);
    if (before <= -0.3f && after >= -0.3f)
        owner.playLevelSound(level, "flap", getPosition(), IDENTIFIER);

    if (isSitting() || --mGrowlTime > 0)
        return;

    owner.playLevelSound(level, "mad", getPosition(), IDENTIFIER);
    mGrowlTime = nextInt(200) + 200;
}

void EnderDragon::_tickCrystals(ServerNetworkHandler &owner, Level &level) {
    EndCrystalActor *crystal = dynamic_cast<EndCrystalActor *>(owner.getActor(mNearestCrystal));
    if (crystal != nullptr && !crystal->isExpired()) {
        if (getLifetimeTicks() % CRYSTAL_HEAL_INTERVAL == 0 && getHealth() < getMaxHealth()) {
            setHealth(std::min(getMaxHealth(), getHealth() + 1.0f));
            owner.syncActorAttributes(*this);
        }
    }

    if (nextInt(10) != 0)
        return;

    const Vector3f position = getPosition();
    const AxisAlignedBB area = ActorPushSystem::boundingBoxOf(*this).expand(CRYSTAL_SEARCH_RANGE,
                                                                            CRYSTAL_SEARCH_RANGE,
                                                                            CRYSTAL_SEARCH_RANGE);
    float best = CRYSTAL_SEARCH_RANGE * CRYSTAL_SEARCH_RANGE;
    int64_t nearest = -1;
    for (auto &entry: owner.getActors()) {
        EndCrystalActor *candidate = dynamic_cast<EndCrystalActor *>(entry.second.get());
        if (candidate == nullptr || candidate->isExpired() || !candidate->isIn(level))
            continue;

        if (!ActorPushSystem::boundingBoxOf(*candidate).intersectsWith(area))
            continue;

        const Vector3f other = candidate->getPosition();
        const float dx = other.x - position.x;
        const float dy = other.y - position.y;
        const float dz = other.z - position.z;
        const float distanceSquared = dx * dx + dy * dy + dz * dz;
        if (best > distanceSquared)
            nearest = candidate->getUniqueId();
        best = std::min(distanceSquared, best);
    }

    mNearestCrystal = nearest;
}

void EnderDragon::_steer(ServerNetworkHandler &owner) {
    (void) owner;
    const Vector3f target = *mTargetLocation;
    const Vector3f position = getPosition();
    Vector3f motion = getMotion();
    Vector3f rotation = getRotation();

    const float dx = target.x - position.x;
    const float dy = target.y - position.y;
    const float dz = target.z - position.z;
    const float distanceSquared = dx * dx + dy * dy + dz * dz;

    const float horizontal = std::sqrt(dz * dz + dx * dx);
    if (horizontal > 0.0f)
        motion.y += clampf(dy / horizontal, -mFlySpeed, mFlySpeed) * 0.1f;

    rotation.y = wrapDegrees(rotation.y);
    const float yaw = rotation.y * DEGREES_TO_RADIANS;
    const float turn = clampf(wrapDegrees(180.0f - std::atan2(dx, dz) * RADIANS_TO_DEGREES - rotation.y), -50.0f,
                              50.0f);

    const float targetLength = std::sqrt(distanceSquared);
    const Vector3f toTarget = targetLength < 0.0001f ? Vector3f(0.0f, 0.0f, 0.0f)
                                                     : Vector3f(dx / targetLength, dy / targetLength,
                                                                dz / targetLength);
    const Vector3f facingRaw(std::sin(yaw), motion.y, -std::cos(yaw));
    const float facingLength = std::sqrt(facingRaw.x * facingRaw.x + facingRaw.y * facingRaw.y
                                         + facingRaw.z * facingRaw.z);
    const Vector3f facing = facingLength < 0.0001f ? Vector3f(0.0f, 0.0f, 0.0f)
                                                   : Vector3f(facingRaw.x / facingLength,
                                                              facingRaw.y / facingLength,
                                                              facingRaw.z / facingLength);
    const float alignment = std::max(0.0f, (facing.x * toTarget.x + facing.y * toTarget.y + facing.z * toTarget.z
                                            + 0.5f) / 1.5f);

    // sub_14ACCA850: a goal without its own turn speed turns at min(speed, 40) / speed
    const float speed = std::sqrt(motion.z * motion.z + motion.x * motion.x) + 1.0f;
    float rate = std::min(speed, 40.0f);
    if (mTurnSpeed != 0.0f)
        rate = mTurnSpeed / rate;
    mYawAcceleration = turn * (rate / speed) + mYawAcceleration * 0.8f;
    rotation.y += mYawAcceleration * 0.1f;

    const float closeness = 2.0f / (distanceSquared + 1.0f);
    const float acceleration = ((1.0f - closeness) + alignment * closeness) * 0.06f;
    const float turned = rotation.y * DEGREES_TO_RADIANS;
    motion.x += std::sin(turned) * acceleration;
    motion.z -= std::cos(turned) * acceleration;

    const float scale = mInWall ? 0.8f : 1.0f;
    setPosition(Vector3f(position.x + motion.x * scale, position.y + motion.y * scale,
                         position.z + motion.z * scale));

    const float motionLength = std::sqrt(motion.x * motion.x + motion.y * motion.y + motion.z * motion.z);
    float heading = 0.0f;
    if (motionLength >= 0.0001f)
        heading = (motion.x * facing.x + motion.y * facing.y + motion.z * facing.z) / motionLength;
    const float friction = (heading + 1.0f) * 0.5f * 0.15f + 0.8f;
    motion.x *= friction;
    motion.y *= 0.91f;
    motion.z *= friction;

    setMotion(motion);
    rotation.z = rotation.y;
    setRotation(rotation);
}

void EnderDragon::_setPart(Part part, float x, float y, float z) {
    const float halfWidth = PART_SIZES[(size_t) part][0] * 0.5f;
    const float halfHeight = PART_SIZES[(size_t) part][1] * 0.5f;
    mParts[(size_t) part] = AxisAlignedBB(x - halfWidth, y - halfHeight, z - halfWidth, x + halfWidth,
                                          y + halfHeight, z + halfWidth);
}

void EnderDragon::_knockBack(ServerNetworkHandler &owner, Level &level, const AxisAlignedBB &area) {
    const Vector3f body = getPartCenter(Part::Body);
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || !player.isIn(level))
            continue;

        if (!ActorPushSystem::boundingBoxOf(player).intersectsWith(area))
            continue;

        const Vector3f position = player.getPosition();
        const float dx = position.x - body.x;
        const float dz = position.z - body.z;
        const float distanceSquared = std::max(dx * dx + dz * dz, 0.0001f);
        const Vector3f motion = player.getMotion();
        player.setMotion(Vector3f(motion.x + dx / distanceSquared * WING_PUSH, motion.y + WING_LIFT,
                                  motion.z + dz / distanceSquared * WING_PUSH));
        owner.sendActorMotion(player);

        if (isSitting() || player.getNoDamageTicks() > 0)
            continue;

        owner.hurt(player, WING_DAMAGE,
                   ActorDamageSource::attack("death.attack.mob", player.getName(), *this, getName(), getPosition()));
    }
}

void EnderDragon::_hurtTouching(ServerNetworkHandler &owner, Level &level, const AxisAlignedBB &area) {
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || !player.isIn(level))
            continue;

        if (!ActorPushSystem::boundingBoxOf(player).intersectsWith(area))
            continue;

        owner.hurt(player, HEAD_DAMAGE,
                   ActorDamageSource::attack("death.attack.mob", player.getName(), *this, getName(), getPosition()));
    }
}

bool EnderDragon::_breakBlocks(ServerNetworkHandler &owner, Level &level, const AxisAlignedBB &area) {
    const int32_t minX = (int32_t) std::floor(area.mMinX);
    const int32_t minY = (int32_t) std::floor(area.mMinY);
    const int32_t minZ = (int32_t) std::floor(area.mMinZ);
    const int32_t maxX = (int32_t) std::floor(area.mMaxX);
    const int32_t maxY = (int32_t) std::floor(area.mMaxY);
    const int32_t maxZ = (int32_t) std::floor(area.mMaxZ);
    const bool griefing = level.getGameRules().getBool("mobgriefing");
    const BlockState air = VanillaBlocks::AIR().toBlockState();

    bool wall = false;
    bool destroyed = false;
    for (int32_t x = minX; x <= maxX; x++) {
        for (int32_t y = minY; y <= maxY; y++) {
            for (int32_t z = minZ; z <= maxZ; z++) {
                const BlockState *state = level.peekBlockPtr(x, y, z);
                if (state == nullptr || passesThrough(state->mName))
                    continue;

                if (!griefing || isImmune(state->mName)) {
                    wall = true;
                    continue;
                }

                const BlockState previous = *state;
                level.setBlock(Vector3i(x, y, z), air, true);
                level.onBlockBroken(Vector3i(x, y, z), previous);
                destroyed = true;
            }
        }
    }

    if (destroyed) {
        const Vector3f particle(area.mMinX + (area.mMaxX - area.mMinX) * nextFloat(),
                                area.mMinY + (area.mMaxY - area.mMinY) * nextFloat(),
                                area.mMinZ + (area.mMaxZ - area.mMinZ) * nextFloat());
        owner.broadcastLevelEvent(level, DESTROY_PARTICLE_EVENT, particle, DESTROY_PARTICLE_DATA);
    }

    return wall;
}

void EnderDragon::_tickParts(ServerNetworkHandler &owner, Level &level) {
    const Vector3f position = getPosition();
    const float yawDegrees = getRotation().y;
    const LatencyPosition latest = _getLatencyPosition(0);
    const LatencyPosition five = _getLatencyPosition(5);
    const LatencyPosition ten = _getLatencyPosition(10);

    const float pitch = (five.mY - ten.mY) * 10.0f * DEGREES_TO_RADIANS;
    const float pitchCos = std::cos(pitch);
    const float pitchSin = std::sin(pitch);
    const float yaw = yawDegrees * DEGREES_TO_RADIANS;
    const float yawSin = std::sin(yaw);
    const float yawCos = std::cos(yaw);
    const float halfHeight = getSize().mHeight * 0.5f;

    _setPart(Part::Body, position.x + yawSin * 0.5f, position.y + halfHeight, position.z - yawCos * 0.5f);
    _setPart(Part::Wing1, position.x + yawCos * 4.5f, position.y + 2.0f, position.z + yawSin * 4.5f);
    _setPart(Part::Wing2, position.x - yawCos * 4.5f, position.y + 2.0f, position.z - yawSin * 4.5f);

    if (!isSitting() && getNoDamageTicks() == 0) {
        const AxisAlignedBB wing1 = mParts[(size_t) Part::Wing1].expand(4.0f, 2.0f, 4.0f).offset(0.0f, -2.0f, 0.0f);
        const AxisAlignedBB wing2 = mParts[(size_t) Part::Wing2].expand(4.0f, 2.0f, 4.0f).offset(0.0f, -2.0f, 0.0f);
        _knockBack(owner, level, wing1);
        _knockBack(owner, level, wing2);
        _hurtTouching(owner, level, mParts[(size_t) Part::Head].expand(1.0f, 1.0f, 1.0f));
        _hurtTouching(owner, level, mParts[(size_t) Part::Neck].expand(1.0f, 1.0f, 1.0f));
    }

    const float headYaw = yaw - mYawAcceleration * 0.01f;
    const float headSin = std::sin(headYaw);
    const float headCos = std::cos(headYaw);
    const float headLift = isSitting() ? -1.0f : five.mY - latest.mY;
    _setPart(Part::Head, position.x + headSin * 6.5f * pitchCos, position.y + headLift + pitchSin * 6.5f,
             position.z - headCos * 6.5f * pitchCos);
    _setPart(Part::Neck, position.x + headSin * 5.5f * pitchCos, position.y + headLift + pitchSin * 5.5f,
             position.z - headCos * 5.5f * pitchCos);

    for (int32_t i = 0; i < 3; i++) {
        const LatencyPosition tail = _getLatencyPosition(12 + i * 2);
        const float tailYaw = yaw + wrapDegrees(tail.mYaw - five.mYaw) * DEGREES_TO_RADIANS;
        const float tailSin = std::sin(tailYaw);
        const float tailCos = std::cos(tailYaw);
        const float length = (float) (i + 1) * 2.0f;
        _setPart((Part) ((int32_t) Part::Tail1 + i),
                 position.x - (yawSin * 1.5f + tailSin * length) * pitchCos,
                 position.y + (tail.mY - five.mY) - (length + 1.5f) * pitchSin + 1.5f,
                 position.z + (yawCos * 1.5f + tailCos * length) * pitchCos);
    }

    // sub_14ACCA850 stops at the first part that hits a wall
    mInWall = _breakBlocks(owner, level, mParts[(size_t) Part::Head])
              || _breakBlocks(owner, level, mParts[(size_t) Part::Neck])
              || _breakBlocks(owner, level, mParts[(size_t) Part::Body]);
}

void EnderDragon::tickMovement(ServerNetworkHandler &owner) {
    if (getHealth() <= 0.0f)
        return;

    Level &level = owner.getLevelFor(*this);
    _tickSounds(owner, level);
    _tickCrystals(owner, level);

    mPreviousFlapTime = mFlapTime;
    const Vector3f motion = getMotion();
    if (isSitting()) {
        mFlapTime += 0.1f;
    } else {
        float flap = std::exp2(motion.y) * (0.2f / (std::sqrt(motion.x * motion.x + motion.z * motion.z) * 10.0f
                                                    + 1.0f));
        if (mInWall)
            flap *= 0.5f;
        mFlapTime += flap;
    }

    Vector3f rotation = getRotation();
    rotation.y = wrapDegrees(rotation.y);
    setRotation(rotation);

    if (mPositionPointer < 0)
        mPositions.fill(LatencyPosition{rotation.y, getPosition().y});
    mPositionPointer = (mPositionPointer + 1) % 64;
    mPositions[(size_t) mPositionPointer] = LatencyPosition{rotation.y, getPosition().y};

    if (mTargetLocation.has_value())
        _steer(owner);

    _tickParts(owner, level);

    if (needsMovementSync()) {
        owner.broadcastActorMove(*this);
        markMovementSynced();
    }
}

void EnderDragon::_sendBossEvent(ServerNetworkHandler &owner, ServerPlayer &player, int32_t action) {
    BossEventPacket packet;
    packet.mBossUniqueActorId = getUniqueId();
    packet.mAction = (BossEventPacket::Action) action;
    packet.mTitle = getName();
    packet.mHealthPercentage = mBossPercent;
    packet.mColor = BOSS_COLOR_PURPLE;
    packet.mOverlay = 0;
    owner.sendPacketTo(player.getNetworkIdentifier(), packet);
}

void EnderDragon::_tickBossBar(ServerNetworkHandler &owner) {
    const json::Value *boss = getComponent("minecraft:boss");
    const json::Value *range = boss == nullptr ? nullptr : boss->get("hud_range");
    const float hudRange = range == nullptr ? BOSS_HUD_RANGE : (float) range->number(BOSS_HUD_RANGE);

    const float percent = isDead() ? 0.0f : clampf(getHealth() / std::max(getMaxHealth(), 1.0f), 0.0f, 1.0f);
    const bool changed = percent != mBossPercent;
    mBossPercent = percent;

    const Vector3f position = getPosition();
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        const Vector3f other = player.getPosition();
        const float dx = other.x - position.x;
        const float dy = other.y - position.y;
        const float dz = other.z - position.z;
        const bool inRange = owner.canPlayerSeeActor(player, *this) && dx * dx + dy * dy + dz * dz <= hudRange
                                                                                                    * hudRange;
        const bool shown = mBossViewers.count(player.getUniqueId()) != 0;

        if (inRange && !shown) {
            _sendBossEvent(owner, player, BOSS_ACTION_CREATE);
            mBossViewers.insert(player.getUniqueId());
        } else if (!inRange && shown) {
            _sendBossEvent(owner, player, BOSS_ACTION_REMOVE);
            mBossViewers.erase(player.getUniqueId());
        } else if (inRange && changed) {
            _sendBossEvent(owner, player, BOSS_ACTION_UPDATE_PERCENT);
        }
    }
}

void EnderDragon::_clearBossBar(ServerNetworkHandler &owner) {
    for (auto &entry: owner.getPlayers()) {
        if (mBossViewers.count(entry.second.getUniqueId()) != 0)
            _sendBossEvent(owner, entry.second, BOSS_ACTION_REMOVE);
    }

    mBossViewers.clear();
}

void EnderDragon::_notifyFight(ServerNetworkHandler &owner) {
    if (mFightNotified)
        return;

    mFightNotified = true;
    Level &level = owner.getLevelFor(*this);
    if (level.getDimensionType() != DimensionType::TheEnd)
        return;

    owner.getWorldFor(*this).getDragonFight().onDragonKilled(owner, level, *this);
}

void EnderDragon::tickDeath(ServerNetworkHandler &owner) {
    Level &level = owner.getLevelFor(*this);
    const int32_t ticks = getDeathTicks();

    // a dying dragon is no longer saved: drop its old record, or a restart now leaves the fight waiting for it
    if (ticks == 1)
        level.eraseEntity(getUniqueId());

    if (mBossPercent != 0.0f) {
        mBossPercent = 0.0f;
        for (auto &entry: owner.getPlayers()) {
            if (mBossViewers.count(entry.second.getUniqueId()) != 0)
                _sendBossEvent(owner, entry.second, BOSS_ACTION_UPDATE_PERCENT);
        }
    }

    const Vector3f position = getPosition();
    setPosition(Vector3f(position.x, position.y + DEATH_RISE, position.z));
    owner.broadcastActorMove(*this);
    markMovementSynced();

    const int32_t experience = EndDragonFight::getDeathExperience(ticks, mPreviouslyKilled);
    if (experience > 0)
        owner.spawnExperienceOrbs(level, getPosition(), experience);

    if (ticks == DEATH_SOUND_TICK)
        owner.playLevelSound(level, "death", getPosition(), IDENTIFIER);

    if (ticks < EndDragonFight::DEATH_DURATION)
        return;

    _clearBossBar(owner);
    _notifyFight(owner);
}

bool EnderDragon::senseDamage(ServerNetworkHandler &owner, float &amount, const ActorDamageSource &source) {
    const Part part = mHurtPart;
    mHurtPart = Part::Body;

    if (!HostileActor::senseDamage(owner, amount, source))
        return false;

    // sub_14ACCD880: an arrow bounces off the sitting dragon and only catches fire
    if (isSitting() && source.mProjectile) {
        if (source.mDamager != nullptr) {
            source.mDamager->setFireTicks(100);
            source.mDamager->setOnFire(true);
        }
        return false;
    }

    if (part != Part::Head && part != Part::Neck)
        amount = std::min(1.0f, amount) + 0.25f * amount;

    if (amount < 0.01f)
        return false;

    // only players and block explosions hurt the dragon
    const bool byPlayer = source.mAttacker != nullptr && source.mAttacker->isPlayer();
    const bool blockExplosion = source.mDeathMessageKey == "death.attack.explosion" && source.mDamager == nullptr;
    if (!byPlayer && !blockExplosion)
        return false;

    mHealthBeforeHurt = getHealth();
    return true;
}

void EnderDragon::onDamaged(ServerNetworkHandler &owner, Actor *attacker) {
    HostileActor::onDamaged(owner, attacker);

    if (getHealth() <= 0.0f) {
        setHealth(1.0f);
        if (!mDying) {
            mDying = true;
            fireTrigger(owner, ON_DEATH_TRIGGER);
        }
    }

    if (!isSitting())
        return;

    mSittingDamage += mHealthBeforeHurt - getHealth();
    if (mSittingDamage > getMaxHealth() * 0.25f) {
        mSittingDamage = 0.0f;
        fireTrigger(owner, ON_TAKEOFF_TRIGGER);
    }
}

bool EnderDragon::hurtPart(ServerNetworkHandler &owner, Part part, float amount, const ActorDamageSource &source) {
    mHurtPart = part;
    const bool hurt = ServerActor::hurt(owner, amount, source);
    mHurtPart = Part::Body;
    return hurt;
}

void EnderDragon::finishDying(ServerNetworkHandler &owner) {
    mDeathRequested = true;
    kill(owner);
}

void EnderDragon::kill(ServerNetworkHandler &owner, ServerPlayer *source, int32_t lootingLevel) {
    if (isDead())
        return;

    // a dragon killed outright, as by /kill, leaves at once and ends the fight (sub_14ACCD880)
    if (!mDeathRequested) {
        _clearBossBar(owner);
        _notifyFight(owner);
        despawn();
        return;
    }

    HostileActor::kill(owner, source, lootingLevel);
}

void EnderDragon::onCrystalDestroyed(ServerNetworkHandler &owner, EndCrystalActor &crystal, Actor *attacker) {
    Actor *source = attacker;
    if (source == nullptr || !source->isPlayer())
        source = nearestPlayer(owner, owner.getLevelFor(*this), crystal.getPosition(), CRYSTAL_ATTACKER_RANGE);

    if (crystal.getUniqueId() == mNearestCrystal) {
        ActorDamageSource damage = ActorDamageSource::environment("death.attack.explosion", getName());
        if (source != nullptr && source->isPlayer())
            damage.mAttacker = source;
        hurtPart(owner, Part::Head, CRYSTAL_DESTROYED_DAMAGE, damage);
    }

    ServerPlayer *player = dynamic_cast<ServerPlayer *>(source);
    if (isSitting() || player == nullptr || !canTarget(*player))
        return;

    setTarget(owner, player->getRuntimeId());
    fireTrigger(owner, ON_TAKEOFF_TRIGGER);
}

void EnderDragon::shootFireball(ServerNetworkHandler &owner, const Actor &target) {
    const Vector3f head = getPartCenter(Part::Head);
    const Vector3f origin(head.x, head.y + 0.5f, head.z);
    const Vector3f aim = target.getPosition();
    const float targetHeight = target.isPlayer() ? 1.8f : 0.0f;
    const float dx = aim.x - origin.x;
    const float dy = aim.y + targetHeight * 0.5f - origin.y;
    const float dz = aim.z - origin.z;
    const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (length < 0.0001f)
        return;

    ServerActor *fireball = owner.spawnActor(owner.getLevelFor(*this), FIREBALL_IDENTIFIER, origin);
    if (fireball == nullptr)
        return;

    fireball->setProjectile(true);
    fireball->setOwnerUniqueId((int64_t) getRuntimeId());
    fireball->setMotion(Vector3f(dx / length * FIREBALL_SPEED, dy / length * FIREBALL_SPEED,
                                 dz / length * FIREBALL_SPEED));
    owner.allowProjectileLaunch(*fireball, this);
}
