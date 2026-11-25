#include "World/Actor/Misc/PaintingActor.h"

#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/BlockStateHasher.h"
#include "Protocol/Packets/AddPaintingPacket.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/TorchBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <string_view>

FALCON_REGISTER_ACTOR(PaintingActor, PaintingActor::IDENTIFIER);

namespace {
    struct Motive {
        const char *mName;
        int32_t mWidth;
        int32_t mHeight;
        bool mPlaceable = true;
    };

    constexpr Motive MOTIVES[] = {
            {"Kebab", 1, 1}, {"Aztec", 1, 1}, {"Alban", 1, 1}, {"Aztec2", 1, 1},
            {"Bomb", 1, 1}, {"Plant", 1, 1}, {"Wasteland", 1, 1}, {"Pool", 2, 1},
            {"Courbet", 2, 1}, {"Sea", 2, 1}, {"Sunset", 2, 1}, {"Creebet", 2, 1},
            {"Wanderer", 1, 2}, {"Graham", 1, 2}, {"Match", 2, 2}, {"Bust", 2, 2},
            {"Stage", 2, 2}, {"Void", 2, 2}, {"SkullAndRoses", 2, 2}, {"Wither", 2, 2},
            {"Fighters", 4, 2}, {"Pointer", 4, 4}, {"Pigscene", 4, 4}, {"BurningSkull", 4, 4},
            {"Skeleton", 4, 3}, {"DonkeyKong", 4, 3}, {"Earth", 2, 2, false}, {"Wind", 2, 2, false},
            {"Fire", 2, 2, false}, {"Water", 2, 2, false}, {"baroque", 2, 2}, {"humble", 2, 2},
            {"meditative", 1, 1}, {"prairie_ride", 1, 2}, {"unpacked", 4, 4}, {"backyard", 3, 4},
            {"bouquet", 3, 3}, {"cavebird", 3, 3}, {"changing", 4, 2}, {"cotan", 3, 3},
            {"endboss", 3, 3}, {"fern", 3, 3}, {"finding", 4, 2}, {"lowmist", 4, 2},
            {"orb", 4, 4}, {"owlemons", 3, 3}, {"passage", 4, 2}, {"pond", 3, 4},
            {"sunflowers", 3, 3}, {"tides", 3, 3}, {"dennis", 3, 3},
    };

    const Vector3i NORMALS[] = {{0, 0, 1}, {-1, 0, 0}, {0, 0, -1}, {1, 0, 0}};
    const Vector3i WIDTH_AXES[] = {{1, 0, 0}, {0, 0, 1}, {-1, 0, 0}, {0, 0, -1}};
    constexpr int32_t FACE_DIRECTIONS[] = {2, 0, 1, 3};
    constexpr int32_t DIRECTION_FACES[] = {3, 4, 2, 5};
    constexpr int32_t BREAK_PARTICLES_EVENT = 2021;
    constexpr int32_t MIN_SUPPORT_CHECK_TICKS = 80;
    constexpr int32_t MAX_SUPPORT_CHECK_TICKS = 119;
    constexpr float WALL_OFFSET = 17.0f / 32.0f;
    constexpr float HALF_THICKNESS = 1.0f / 32.0f;
    constexpr int32_t LEGACY_DIRECTIONS[] = {2, 1, 0, 3};
    constexpr float YAWS[] = {180.0f, 90.0f, 0.0f, 270.0f};

    size_t motiveIndex(std::string_view name) {
        const auto motive = std::find_if(std::begin(MOTIVES), std::end(MOTIVES), [name](const Motive &entry) {
            return entry.mName == name;
        });
        return motive != std::end(MOTIVES) ? (size_t) (motive - std::begin(MOTIVES)) : 0;
    }

    Vector3f centerOf(const Vector3i &support, int32_t direction, const Motive &motive) {
        const Vector3i &normal = NORMALS[direction];
        const Vector3i &across = WIDTH_AXES[direction];
        const float shift = motive.mWidth % 2 == 0 ? 0.5f : 0.0f;
        return Vector3f(support.x + 0.5f + normal.x * WALL_OFFSET + across.x * shift,
                        support.y + (motive.mHeight % 2 == 0 ? 1.0f : 0.5f),
                        support.z + 0.5f + normal.z * WALL_OFFSET + across.z * shift);
    }

    AxisAlignedBB boundsOf(const Vector3f &center, int32_t direction, const Motive &motive) {
        const float x = NORMALS[direction].x == 0 ? motive.mWidth * 0.5f - HALF_THICKNESS : HALF_THICKNESS;
        const float y = motive.mHeight * 0.5f - HALF_THICKNESS;
        const float z = NORMALS[direction].z == 0 ? motive.mWidth * 0.5f - HALF_THICKNESS : HALF_THICKNESS;
        return AxisAlignedBB(center.x - x, center.y - y, center.z - z, center.x + x, center.y + y, center.z + z);
    }

    enum class WallFit { Fits, Blocked, Unloaded };

    WallFit fitsWall(Level &level, const Vector3f &center, int32_t direction, const Motive &motive, bool placing) {
        const Vector3i &normal = NORMALS[direction];
        const auto bounds = boundsOf(center, direction, motive);
        const int32_t x = (int32_t) std::floor(normal.x == 0 ? bounds.mMinX : center.x - normal.x * WALL_OFFSET);
        const int32_t y = (int32_t) std::floor(bounds.mMinY);
        const int32_t z = (int32_t) std::floor(normal.z == 0 ? bounds.mMinZ : center.z - normal.z * WALL_OFFSET);
        if (y < level.getMinY() || y + motive.mHeight - 1 > level.getMaxY())
            return WallFit::Blocked;

        bool unloaded = false;
        for (int32_t width = 0; width < motive.mWidth; ++width) {
            for (int32_t height = 0; height < motive.mHeight; ++height) {
                const int32_t blockX = x + (normal.x == 0 ? width : 0);
                const int32_t blockZ = z + (normal.z == 0 ? width : 0);
                const auto *support = level.peekBlockPtr(blockX, y + height, blockZ);
                const auto *front = level.peekBlockPtr(blockX + normal.x, y + height, blockZ + normal.z);
                if (support == nullptr || front == nullptr) {
                    unloaded = true;
                    continue;
                }
                if (!BlockSupport::isSolid(*support))
                    return WallFit::Blocked;
                if (placing && VanillaBlocks::getAs<TorchBlock>(front->mName) != nullptr
                    && PlacementOrientation::faceFromTorchFacing(front->mStates.getString("torch_facing_direction"))
                               == DIRECTION_FACES[direction])
                    return WallFit::Blocked;
            }
        }
        if (!level.getCollisionBoxes(bounds).empty())
            return WallFit::Blocked;
        return unloaded ? WallFit::Unloaded : WallFit::Fits;
    }
}

PaintingActor::PaintingActor(uint64_t runtimeId, const std::string &identifier)
        : ServerActor(runtimeId, identifier) {
    addTypeFlag(ActorTypeFlag::PaintingFlagComponent);
}

bool PaintingActor::place(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &support, int32_t face) {
    if (face < 2 || face > 5 || player.isSpectator())
        return false;

    const int32_t direction = FACE_DIRECTIONS[face - 2];
    Level &level = owner.getLevelFor(player);
    std::array<const Motive *, std::size(MOTIVES)> best{};
    int32_t area = 0;
    size_t count = 0;
    static thread_local std::mt19937 random(std::random_device{}());
    for (const Motive &motive: MOTIVES) {
        const int32_t size = motive.mWidth * motive.mHeight;
        if (!motive.mPlaceable || size < area)
            continue;

        const auto center = centerOf(support, direction, motive);
        if (fitsWall(level, center, direction, motive, true) != WallFit::Fits)
            continue;

        const auto bounds = boundsOf(center, direction, motive);
        bool overlaps = false;
        owner.forEachActorNear(center, 4.0f, [&](ServerActor &actor) {
            const auto *painting = dynamic_cast<const PaintingActor *>(&actor);
            if (painting != nullptr && !painting->isExpired() && painting->isIn(level)
                && bounds.intersectsWith(painting->getPaintingBounds()))
                overlaps = true;
        });
        if (overlaps)
            continue;

        if (size > area) {
            area = size;
            count = 0;
        }
        best[count++] = &motive;
    }
    if (count == 0)
        return false;

    const Motive *selected = best[std::uniform_int_distribution<size_t>(0, count - 1)(random)];
    return owner.spawnActor(level, IDENTIFIER, centerOf(support, direction, *selected), [&](ServerActor &actor) {
        if (auto *painting = dynamic_cast<PaintingActor *>(&actor)) {
            painting->mDirection = direction;
            painting->mMotive = (size_t) (selected - MOTIVES);
            painting->setRotation(Vector3f(0.0f, YAWS[direction], YAWS[direction]));
        }
    }) != nullptr;
}

ActorSize PaintingActor::getSize() const {
    const auto &motive = MOTIVES[mMotive];
    return ActorSize{(float) motive.mWidth, (float) motive.mHeight};
}

AxisAlignedBB PaintingActor::getPaintingBounds() const {
    return boundsOf(getPosition(), mDirection, MOTIVES[mMotive]);
}

void PaintingActor::sendPaintingSpawn(ServerNetworkHandler &owner, const ServerPlayer &player) const {
    AddPaintingPacket packet;
    packet.mUniqueActorId = getUniqueId();
    packet.mRuntimeActorId = getRuntimeId();
    packet.mPosition = getPosition();
    packet.mDirection = mDirection;
    packet.mMotive = MOTIVES[mMotive].mName;
    owner.sendPacketTo(player.getNetworkIdentifier(), packet);
}

void PaintingActor::tick(ServerNetworkHandler &owner) {
    if (mExpired || mSupportCheckTicks-- > 0)
        return;
    static thread_local std::mt19937 random(std::random_device{}());
    mSupportCheckTicks = std::uniform_int_distribution<int32_t>(MIN_SUPPORT_CHECK_TICKS, MAX_SUPPORT_CHECK_TICKS)(random);
    if (fitsWall(owner.getLevelFor(*this), getPosition(), mDirection, MOTIVES[mMotive], false) == WallFit::Blocked)
        kill(owner);
}

void PaintingActor::tickMovement(ServerNetworkHandler &) {
}

bool PaintingActor::isExpired() const {
    return mExpired;
}

bool PaintingActor::shouldSave() const {
    return !mExpired && ServerActor::shouldSave();
}

bool PaintingActor::isPushable() const {
    return false;
}

bool PaintingActor::onHurt(ServerNetworkHandler &owner, float amount, ServerPlayer *source) {
    if (amount > 0.0f)
        kill(owner, source);
    return true;
}

void PaintingActor::kill(ServerNetworkHandler &owner, ServerPlayer *source, int32_t) {
    if (mExpired)
        return;
    mExpired = true;
    Level &level = owner.getLevelFor(*this);
    owner.broadcastLevelEvent(level, LevelEventPacket::SoundItemFrameBreak, getPosition(), 0);
    const BlockState oak("minecraft:oak_planks");
    owner.broadcastLevelEvent(level, BREAK_PARTICLES_EVENT, getPosition(),
                              (int32_t) BlockStateHasher::hash(oak.mName, oak.mStates));
    if ((source == nullptr || !source->isCreative()) && level.getGameRules().getBool("doentitydrops")) {
        owner.dropItem(level, getPosition(), owner.createItemStack(IDENTIFIER, 1), ItemActorHandler::randomDropMotion(),
                       ItemActorHandler::DROP_PICKUP_DELAY);
    }
}

Tag PaintingActor::saveNbt() const {
    Tag data = ServerActor::saveNbt();
    data.putString("Motif", MOTIVES[mMotive].mName);
    data.putByte("Dir", (int8_t) LEGACY_DIRECTIONS[mDirection]);
    data.putByte("Direction", (int8_t) mDirection);
    return data;
}

void PaintingActor::loadNbt(const Tag &data) {
    ServerActor::loadNbt(data);
    mMotive = motiveIndex(data.getString("Motive", data.getString("Motif")));
    const int32_t direction = data.getByte("Direction", -1);
    const int32_t legacy = std::clamp<int32_t>(data.getByte("Dir"), 0, 3);
    mDirection = direction >= 0 && direction < 4 ? direction : LEGACY_DIRECTIONS[legacy];
    setRotation(Vector3f(0.0f, YAWS[mDirection], YAWS[mDirection]));
}
