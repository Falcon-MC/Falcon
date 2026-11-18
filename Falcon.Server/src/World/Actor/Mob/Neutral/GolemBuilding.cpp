#include "World/Actor/Mob/Neutral/GolemBuilding.h"

#include "Core/Math/Vector3f.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <string>

namespace {
    const char *const SNOW_GOLEM = "minecraft:snow_golem";
    const char *const IRON_GOLEM = "minecraft:iron_golem";
    const char *const FROM_PLAYER_EVENT = "minecraft:from_player";
    const int32_t LEVEL_EVENT_DESTROY_NO_SOUND = 2021;

    // rows from the first one along the up direction; '^' a head, '#' a body block, '~' air
    const char *const SNOW_ROWS[] = {"^", "#", "#"};
    const char *const IRON_ROWS[] = {"~^~", "###", "~#~"};
    const int32_t ROW_COUNT = 3;

    // down, up, north, south, west, east
    const Vector3i FACINGS[6] = {Vector3i(0, -1, 0), Vector3i(0, 1, 0), Vector3i(0, 0, -1),
                                 Vector3i(0, 0, 1),  Vector3i(-1, 0, 0), Vector3i(1, 0, 0)};
    const int32_t FACING_UP = 1;
    const int32_t FACING_NORTH = 2;
    const int32_t FACING_SOUTH = 3;
    const float NORTH_SOUTH_YAW = 90.0f;

    bool isHead(const BlockState &state) {
        return state.mName == BlockNames::CARVED_PUMPKIN || state.mName == BlockNames::LIT_PUMPKIN;
    }

    bool cellMatches(char cell, const char *body, const BlockState &state) {
        if (cell == '^')
            return isHead(state);
        if (cell == '#')
            return state.mName == body;
        return state.mName == BlockNames::AIR;
    }

    int32_t widthOf(const char *const *rows) {
        return (int32_t) std::string(rows[0]).size();
    }

    Vector3i cellAt(const Vector3i &origin, int32_t row, int32_t column, int32_t forwards, int32_t up) {
        const Vector3i &f = FACINGS[forwards];
        const Vector3i &u = FACINGS[up];
        const Vector3i side(f.y * u.z - f.z * u.y, f.z * u.x - f.x * u.z, f.x * u.y - f.y * u.x);
        return Vector3i(origin.x + u.x * row + side.x * column, origin.y + u.y * row + side.y * column,
                        origin.z + u.z * row + side.z * column);
    }

    bool matchesAt(Level &level, const char *const *rows, const char *body, const Vector3i &origin, int32_t forwards,
                   int32_t up) {
        const int32_t width = widthOf(rows);
        for (int32_t column = 0; column < width; ++column) {
            for (int32_t row = 0; row < ROW_COUNT; ++row) {
                const Vector3i cell = cellAt(origin, row, column, forwards, up);
                const BlockState *state = level.peekBlockPtr(cell.x, cell.y, cell.z);
                if (state == nullptr || !cellMatches(rows[row][column], body, *state))
                    return false;
            }
        }
        return true;
    }

    // forwards then up, every pair where up is not along forwards
    bool matchesAnyWay(Level &level, const char *const *rows, const char *body, const Vector3i &origin,
                       GolemBuilding::Match &match) {
        for (int32_t forwards = 0; forwards < 6; ++forwards) {
            for (int32_t up = 0; up < 6; ++up) {
                if (up / 2 == forwards / 2 || !matchesAt(level, rows, body, origin, forwards, up))
                    continue;
                match.mOrigin = origin;
                match.mForwards = forwards;
                match.mUp = up;
                return true;
            }
        }
        return false;
    }

    bool fitsAnyCell(const char *const *rows, const char *body, const BlockState &state) {
        const int32_t width = widthOf(rows);
        for (int32_t row = 0; row < ROW_COUNT; ++row) {
            for (int32_t column = 0; column < width; ++column) {
                if (cellMatches(rows[row][column], body, state))
                    return true;
            }
        }
        return false;
    }

    void clearPattern(ServerNetworkHandler &owner, Level &level, const char *const *rows,
                      const GolemBuilding::Match &match) {
        const int32_t width = widthOf(rows);
        for (int32_t column = 0; column < width; ++column) {
            for (int32_t row = 0; row < ROW_COUNT; ++row) {
                const Vector3i cell = cellAt(match.mOrigin, row, column, match.mForwards, match.mUp);
                const BlockState state = level.getBlockState(cell.x, cell.y, cell.z);
                if (state.mName != BlockNames::AIR)
                    owner.broadcastLevelEvent(level, LEVEL_EVENT_DESTROY_NO_SOUND,
                                              Vector3f((float) cell.x, (float) cell.y, (float) cell.z),
                                              (int32_t) BlockStateHasher::hash(state.mName, state.mStates));
                level.setBlock(cell, BlockState(BlockNames::AIR), true);
            }
        }
    }
}

GolemBuilding::Match GolemBuilding::find(Level &level, const Vector3i &pumpkin) {
    Match match;
    if (matchesAnyWay(level, SNOW_ROWS, BlockNames::SNOW, pumpkin, match)) {
        match.mKind = Kind::Snow;
        return match;
    }

    // the game only scans the corner of the box at or below the placed block on every axis
    const int32_t reach = std::max(ROW_COUNT, widthOf(IRON_ROWS));
    for (int32_t x = pumpkin.x - reach; x <= pumpkin.x; ++x) {
        for (int32_t y = pumpkin.y - reach; y <= pumpkin.y; ++y) {
            for (int32_t z = pumpkin.z - reach; z <= pumpkin.z; ++z) {
                const BlockState *state = level.peekBlockPtr(x, y, z);
                if (state == nullptr || !fitsAnyCell(IRON_ROWS, BlockNames::IRON_BLOCK, *state))
                    continue;
                if (matchesAnyWay(level, IRON_ROWS, BlockNames::IRON_BLOCK, Vector3i(x, y, z), match)) {
                    match.mKind = Kind::Iron;
                    return match;
                }
            }
        }
    }
    return Match();
}

bool GolemBuilding::tryBuild(ServerNetworkHandler &owner, Level &level, const Vector3i &pumpkin) {
    const Match match = find(level, pumpkin);
    if (match.mKind == Kind::None)
        return false;

    // the game spawns on the bottom row: two below the first one, shifted back up unless the pattern stands upright
    const bool iron = match.mKind == Kind::Iron;
    const float lift = match.mUp >= FACING_UP ? 2.0f : 0.0f;
    const Vector3f position = iron ? Vector3f((float) match.mOrigin.x + 1.0f, (float) match.mOrigin.y - 2.0f + lift,
                                              (float) match.mOrigin.z + 1.0f)
                                   : Vector3f((float) match.mOrigin.x + 0.5f,
                                              (float) match.mOrigin.y - 2.0f + 0.05f + lift,
                                              (float) match.mOrigin.z + 0.5f);
    ServerActor *golem = owner.spawnActor(level, iron ? IRON_GOLEM : SNOW_GOLEM, position, [iron](ServerActor &actor) {
        MobActor *mob = dynamic_cast<MobActor *>(&actor);
        if (iron && mob != nullptr)
            mob->setSpawnEvent(FROM_PLAYER_EVENT);
    });
    if (golem == nullptr)
        return false;

    if (iron && (match.mForwards == FACING_NORTH || match.mForwards == FACING_SOUTH)) {
        Vector3f rotation = golem->getRotation();
        rotation.y += NORTH_SOUTH_YAW;
        rotation.z += NORTH_SOUTH_YAW;
        golem->setRotation(rotation);
    }

    clearPattern(owner, level, iron ? IRON_ROWS : SNOW_ROWS, match);
    return true;
}
