#include "World/Actor/Mob/Neutral/GolemBuilding.h"

#include "Core/Math/Vector3f.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <string>

namespace {
    const char *const SNOW_GOLEM = "minecraft:snow_golem";
    const char *const IRON_GOLEM = "minecraft:iron_golem";
    const char *const COPPER_GOLEM = "minecraft:copper_golem";
    const char *const FROM_PLAYER_EVENT = "minecraft:from_player";
    const char *const CARDINAL_DIRECTION = "minecraft:cardinal_direction";
    const int32_t LEVEL_EVENT_DESTROY_NO_SOUND = 2021;

    // rows from the first one along the up direction; '^' a head, '#' a body block, '~' air
    const char *const SNOW_ROWS[] = {"^", "#", "#"};
    const char *const IRON_ROWS[] = {"~^~", "###", "~#~"};
    const char *const COPPER_ROWS[] = {"^", "#"};
    const int32_t ROW_COUNT = 3;
    const int32_t COPPER_ROW_COUNT = 2;

    struct CopperStage {
        const char *mEvent;
        const char *mChest;
    };

    // the game reads the stage from the body block: a waxed block counts as its unwaxed stage
    const CopperStage COPPER_STAGES[] = {
            {"minecraft:from_player_default", BlockNames::COPPER_CHEST},
            {"minecraft:from_player_exposed", BlockNames::EXPOSED_COPPER_CHEST},
            {"minecraft:from_player_weathered", BlockNames::WEATHERED_COPPER_CHEST},
            {"minecraft:from_player_oxidized", BlockNames::OXIDIZED_COPPER_CHEST}};
    const char *const COPPER_BODIES[][2] = {{BlockNames::COPPER_BLOCK, BlockNames::WAXED_COPPER},
                                            {BlockNames::EXPOSED_COPPER, BlockNames::WAXED_EXPOSED_COPPER},
                                            {BlockNames::WEATHERED_COPPER, BlockNames::WAXED_WEATHERED_COPPER},
                                            {BlockNames::OXIDIZED_COPPER, BlockNames::WAXED_OXIDIZED_COPPER}};
    const int32_t COPPER_STAGE_COUNT = 4;

    // south, west, north, east: the yaw the golem faces for each cardinal_direction of its head
    const char *const CARDINALS[] = {"south", "west", "north", "east"};
    const float CARDINAL_YAWS[] = {0.0f, 90.0f, -180.0f, -90.0f};

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

    int32_t copperStageOf(const BlockState &state) {
        for (int32_t stage = 0; stage < COPPER_STAGE_COUNT; ++stage) {
            if (state.mName == COPPER_BODIES[stage][0] || state.mName == COPPER_BODIES[stage][1])
                return stage;
        }
        return -1;
    }

    struct Shape {
        const char *const *mRows;
        int32_t mRowCount;
        // nullptr: any copper block
        const char *mBody;
    };

    const Shape SNOW_SHAPE{SNOW_ROWS, ROW_COUNT, BlockNames::SNOW};
    const Shape IRON_SHAPE{IRON_ROWS, ROW_COUNT, BlockNames::IRON_BLOCK};
    const Shape COPPER_SHAPE{COPPER_ROWS, COPPER_ROW_COUNT, nullptr};

    bool cellMatches(char cell, const Shape &shape, const BlockState &state) {
        if (cell == '^')
            return isHead(state);
        if (cell == '#')
            return shape.mBody == nullptr ? copperStageOf(state) >= 0 : state.mName == shape.mBody;
        return state.mName == BlockNames::AIR;
    }

    int32_t widthOf(const Shape &shape) {
        return (int32_t) std::string(shape.mRows[0]).size();
    }

    Vector3i cellAt(const Vector3i &origin, int32_t row, int32_t column, int32_t forwards, int32_t up) {
        const Vector3i &f = FACINGS[forwards];
        const Vector3i &u = FACINGS[up];
        const Vector3i side(f.y * u.z - f.z * u.y, f.z * u.x - f.x * u.z, f.x * u.y - f.y * u.x);
        return Vector3i(origin.x + u.x * row + side.x * column, origin.y + u.y * row + side.y * column,
                        origin.z + u.z * row + side.z * column);
    }

    bool matchesAt(Level &level, const Shape &shape, const Vector3i &origin, int32_t forwards, int32_t up) {
        const int32_t width = widthOf(shape);
        for (int32_t column = 0; column < width; ++column) {
            for (int32_t row = 0; row < shape.mRowCount; ++row) {
                const Vector3i cell = cellAt(origin, row, column, forwards, up);
                const BlockState *state = level.peekBlockPtr(cell.x, cell.y, cell.z);
                if (state == nullptr || !cellMatches(shape.mRows[row][column], shape, *state))
                    return false;
            }
        }
        return true;
    }

    // forwards then up, every pair where up is not along forwards
    bool matchesAnyWay(Level &level, const Shape &shape, const Vector3i &origin, GolemBuilding::Match &match) {
        for (int32_t forwards = 0; forwards < 6; ++forwards) {
            for (int32_t up = 0; up < 6; ++up) {
                if (up / 2 == forwards / 2 || !matchesAt(level, shape, origin, forwards, up))
                    continue;
                match.mOrigin = origin;
                match.mForwards = forwards;
                match.mUp = up;
                return true;
            }
        }
        return false;
    }

    bool fitsAnyCell(const Shape &shape, const BlockState &state) {
        const int32_t width = widthOf(shape);
        for (int32_t row = 0; row < shape.mRowCount; ++row) {
            for (int32_t column = 0; column < width; ++column) {
                if (cellMatches(shape.mRows[row][column], shape, state))
                    return true;
            }
        }
        return false;
    }

    // body cells take the replacement, the others turn to air
    void clearPattern(ServerNetworkHandler &owner, Level &level, const Shape &shape, const GolemBuilding::Match &match,
                      const BlockState &body) {
        const int32_t width = widthOf(shape);
        for (int32_t column = 0; column < width; ++column) {
            for (int32_t row = 0; row < shape.mRowCount; ++row) {
                const Vector3i cell = cellAt(match.mOrigin, row, column, match.mForwards, match.mUp);
                const BlockState state = level.getBlockState(cell.x, cell.y, cell.z);
                if (state.mName != BlockNames::AIR)
                    owner.broadcastLevelEvent(level, LEVEL_EVENT_DESTROY_NO_SOUND,
                                              Vector3f((float) cell.x, (float) cell.y, (float) cell.z),
                                              (int32_t) BlockStateHasher::hash(state.mName, state.mStates));
                level.setBlock(cell, shape.mRows[row][column] == '#' ? body : BlockState(BlockNames::AIR), true);
            }
        }
    }

    // the game only scans the corner of the box at or below the placed block on every axis
    bool scanBox(Level &level, const Shape &shape, const Vector3i &pumpkin, GolemBuilding::Match &match) {
        const int32_t reach = std::max(shape.mRowCount, widthOf(shape));
        for (int32_t x = pumpkin.x - reach; x <= pumpkin.x; ++x) {
            for (int32_t y = pumpkin.y - reach; y <= pumpkin.y; ++y) {
                for (int32_t z = pumpkin.z - reach; z <= pumpkin.z; ++z) {
                    const BlockState *state = level.peekBlockPtr(x, y, z);
                    if (state == nullptr || !fitsAnyCell(shape, *state))
                        continue;
                    if (matchesAnyWay(level, shape, Vector3i(x, y, z), match))
                        return true;
                }
            }
        }
        return false;
    }

    int32_t cardinalOf(const BlockState &state) {
        const std::string direction = state.mStates.getString(CARDINAL_DIRECTION, CARDINALS[0]);
        for (int32_t index = 0; index < 4; ++index) {
            if (direction == CARDINALS[index])
                return index;
        }
        return 0;
    }

    bool buildCopper(ServerNetworkHandler &owner, Level &level, const GolemBuilding::Match &match) {
        const Vector3i &head = match.mOrigin;
        const Vector3i &step = FACINGS[match.mUp];
        const Vector3i bodyCell(head.x + step.x, head.y + step.y, head.z + step.z);
        const int32_t stage = copperStageOf(level.getBlockState(bodyCell.x, bodyCell.y, bodyCell.z));
        const int32_t cardinal = cardinalOf(level.getBlockState(head.x, head.y, head.z));
        if (stage < 0)
            return false;

        const Block *chest = VanillaBlocks::fromIdentifier(COPPER_STAGES[stage].mChest);
        BlockState body = chest != nullptr ? chest->toBlockState() : BlockState(COPPER_STAGES[stage].mChest);
        body.mStates.putString(CARDINAL_DIRECTION, CARDINALS[cardinal]);

        // the golem stands in the head's cell, turned the way the head faces
        const Vector3f position((float) head.x + 0.5f, (float) head.y, (float) head.z + 0.5f);
        const char *event = COPPER_STAGES[stage].mEvent;
        const float yaw = CARDINAL_YAWS[cardinal];
        ServerActor *golem = owner.spawnActor(level, COPPER_GOLEM, position, [event, yaw](ServerActor &actor) {
            actor.setRotation(Vector3f(0.0f, yaw, yaw));
            MobActor *mob = dynamic_cast<MobActor *>(&actor);
            if (mob != nullptr)
                mob->setSpawnEvent(event);
        });
        if (golem == nullptr)
            return false;

        clearPattern(owner, level, COPPER_SHAPE, match, body);
        return true;
    }
}

GolemBuilding::Match GolemBuilding::find(Level &level, const Vector3i &pumpkin) {
    Match match;
    if (matchesAnyWay(level, SNOW_SHAPE, pumpkin, match)) {
        match.mKind = Kind::Snow;
        return match;
    }
    if (scanBox(level, IRON_SHAPE, pumpkin, match)) {
        match.mKind = Kind::Iron;
        return match;
    }
    if (scanBox(level, COPPER_SHAPE, pumpkin, match)) {
        match.mKind = Kind::Copper;
        return match;
    }
    return Match();
}

bool GolemBuilding::tryBuild(ServerNetworkHandler &owner, Level &level, const Vector3i &pumpkin) {
    const Match match = find(level, pumpkin);
    if (match.mKind == Kind::None)
        return false;
    if (match.mKind == Kind::Copper)
        return buildCopper(owner, level, match);

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

    clearPattern(owner, level, iron ? IRON_SHAPE : SNOW_SHAPE, match, BlockState(BlockNames::AIR));
    return true;
}
