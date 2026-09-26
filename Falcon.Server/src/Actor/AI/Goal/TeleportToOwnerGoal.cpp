#include "Actor/AI/Goal/TeleportToOwnerGoal.h"

#include "Actor/Definition/EntityFilter.h"
#include "Actor/Mob/MobActor.h"
#include "Actor/ServerPlayer.h"
#include "Block/BlockData.h"
#include "Block/Systems/LiquidBlocksFetch.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <cstdlib>
#include <random>
#include <utility>

namespace {
    const int32_t ATTEMPTS = 10;
    const int32_t HORIZONTAL_RANGE = 3;
    const int32_t VERTICAL_RANGE = 1;
    const int32_t MIN_OWNER_OFFSET = 2;

    std::mt19937 &teleportRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    int32_t offset(int32_t range) {
        return std::uniform_int_distribution<int32_t>(-range, range)(teleportRandom());
    }

    bool isSolid(Level &level, int32_t x, int32_t y, int32_t z) {
        const BlockState *state = level.peekBlockPtr(x, y, z);
        const BlockData *data = state == nullptr ? nullptr : BlockDataTable::find(state->mName.c_str());
        return data != nullptr && data->mSolid;
    }
}

TeleportToOwnerGoal::TeleportToOwnerGoal(std::shared_ptr<json::Value> filters) : mFilters(std::move(filters)) {
}

bool TeleportToOwnerGoal::_isSafe(Level &level, int32_t x, int32_t y, int32_t z) {
    if (level.peekBlockPtr(x, y, z) == nullptr || !isSolid(level, x, y - 1, z))
        return false;
    if (isSolid(level, x, y, z) || isSolid(level, x, y + 1, z))
        return false;

    const LiquidContact feet = LiquidBlocksFetch::at(level, Vector3f((float) x + 0.5f, (float) y, (float) z + 0.5f));
    return !feet.water && !feet.lava;
}

bool TeleportToOwnerGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (mob.isSitting() || mob.isRiding() || mob.getOwner(owner) == nullptr)
        return false;

    return mFilters == nullptr || EntityFilter::test(*mFilters, owner, mob);
}

bool TeleportToOwnerGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return false;
}

void TeleportToOwnerGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    const ServerPlayer *player = mob.getOwner(owner);
    if (player == nullptr)
        return;

    Level &level = owner.getLevelFor(mob);
    const Vector3f ownerPosition = player->getPosition();
    const int32_t ownerX = (int32_t) std::floor(ownerPosition.x);
    const int32_t ownerY = (int32_t) std::floor(ownerPosition.y);
    const int32_t ownerZ = (int32_t) std::floor(ownerPosition.z);

    for (int32_t attempt = 0; attempt < ATTEMPTS; ++attempt) {
        const int32_t dx = offset(HORIZONTAL_RANGE);
        const int32_t dz = offset(HORIZONTAL_RANGE);
        if (std::abs(dx) < MIN_OWNER_OFFSET && std::abs(dz) < MIN_OWNER_OFFSET)
            continue;

        const int32_t x = ownerX + dx;
        const int32_t y = ownerY + offset(VERTICAL_RANGE);
        const int32_t z = ownerZ + dz;
        if (!_isSafe(level, x, y, z))
            continue;

        mob.getNavigation().stop(mob);
        mob.teleport(Vector3f((float) x + 0.5f, (float) y, (float) z + 0.5f));
        return;
    }
}
