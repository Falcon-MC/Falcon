#include "World/Level/LevelGen/Structure/MansionLayout.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace {
    using MansionLayout::Cell;
    using MansionLayout::Floor;
    using MansionLayout::SIZE;

    enum Direction { North, South, West, East };
    constexpr std::array<int32_t, 4> DX = {0, 0, -1, 1};
    constexpr std::array<int32_t, 4> DZ = {-1, 1, 0, 0};

    bool contains(int32_t x, int32_t z) {
        return x >= 0 && x < SIZE && z >= 0 && z < SIZE;
    }

    Cell get(const Floor &floor, int32_t x, int32_t z) {
        return contains(x, z) ? floor[z][x] : Cell::Outside;
    }

    void fill(Floor &floor, int32_t x0, int32_t z0, int32_t x1, int32_t z1, Cell cell) {
        for (int32_t z = z0; z <= z1; ++z) {
            for (int32_t x = x0; x <= x1; ++x)
                floor[z][x] = cell;
        }
    }

    void setIfEmpty(Floor &floor, int32_t x, int32_t z, Cell cell) {
        if (get(floor, x, z) == Cell::Empty)
            floor[z][x] = cell;
    }

    void carve(Floor &floor, std::mt19937 &random, int32_t x, int32_t z, Direction direction, int32_t depth) {
        if (depth == 0)
            return;

        floor[z][x] = Cell::Corridor;
        const int32_t dx = DX[direction];
        const int32_t dz = DZ[direction];
        const int32_t nextX = x + dx;
        const int32_t nextZ = z + dz;
        setIfEmpty(floor, nextX, nextZ, Cell::Corridor);

        for (int32_t attempt = 0; attempt < 8; ++attempt) {
            const auto branch = static_cast<Direction>(random() % 4);
            if (branch == (direction ^ 1) || (branch == East && (random() & 0x08000000U) != 0))
                continue;
            const int32_t bx = DX[branch];
            const int32_t bz = DZ[branch];
            if (get(floor, nextX + bx, nextZ + bz) == Cell::Empty
                && get(floor, nextX + 2 * bx, nextZ + 2 * bz) == Cell::Empty) {
                carve(floor, random, nextX + bx, nextZ + bz, branch, depth - 1);
                break;
            }
        }

        for (int32_t side : {-1, 1}) {
            const int32_t sx = side * dz;
            const int32_t sz = side * dx;
            setIfEmpty(floor, x + sx, z + sz, Cell::Room);
            setIfEmpty(floor, nextX + sx, nextZ + sz, Cell::Room);
            setIfEmpty(floor, x + 2 * sx, z + 2 * sz, Cell::Room);
        }
        setIfEmpty(floor, x + 2 * dx, z + 2 * dz, Cell::Room);
    }

    int32_t occupied(const Floor &floor, int32_t x, int32_t z) {
        const auto cell = get(floor, x, z);
        return cell != Cell::Empty && cell != Cell::Outside;
    }

    bool fillGaps(Floor &floor) {
        bool changed = false;
        for (int32_t z = 0; z < SIZE; ++z) {
            for (int32_t x = 0; x < SIZE; ++x) {
                if (floor[z][x] != Cell::Empty)
                    continue;
                const int32_t sides = occupied(floor, x - 1, z) + occupied(floor, x + 1, z)
                                      + occupied(floor, x, z - 1) + occupied(floor, x, z + 1);
                if (sides < 2)
                    continue;
                const int32_t diagonals = occupied(floor, x - 1, z - 1) + occupied(floor, x + 1, z - 1)
                                          + occupied(floor, x - 1, z + 1) + occupied(floor, x + 1, z + 1);
                if (sides > 2 || diagonals < 2) {
                    floor[z][x] = Cell::Room;
                    changed = true;
                }
            }
        }
        return changed;
    }

    struct RoomBounds {
        int32_t x0, z0, x1, z1;
        MansionLayout::RoomSize size;
    };

    bool isAvailable(const Floor &floor, const MansionLayout::Rooms &rooms, const RoomBounds &bounds) {
        for (int32_t z = bounds.z0; z <= bounds.z1; ++z) {
            for (int32_t x = bounds.x0; x <= bounds.x1; ++x) {
                if (get(floor, x, z) != Cell::Room || rooms[z][x].mId != 0)
                    return false;
            }
        }
        return true;
    }

    RoomBounds pickRoom(const Floor &floor, const MansionLayout::Rooms &rooms, int32_t x, int32_t z) {
        using MansionLayout::RoomSize;
        const std::array<RoomBounds, 7> candidates = {{
                {x, z, x + 1, z + 1, RoomSize::Square},
                {x - 1, z, x, z + 1, RoomSize::Square},
                {x - 1, z - 1, x, z, RoomSize::Square},
                {x, z, x + 1, z, RoomSize::Double},
                {x, z, x, z + 1, RoomSize::Double},
                {x - 1, z, x, z, RoomSize::Double},
                {x, z - 1, x, z, RoomSize::Double},
        }};
        for (const RoomBounds &candidate : candidates) {
            if (isAvailable(floor, rooms, candidate))
                return candidate;
        }
        return {x, z, x, z, RoomSize::Single};
    }

    bool touchesCorridor(const Floor &floor, int32_t x, int32_t z) {
        for (int32_t direction = North; direction <= East; ++direction) {
            if (get(floor, x + DX[direction], z + DZ[direction]) == Cell::Corridor)
                return true;
        }
        return false;
    }
}

MansionLayout::Floor MansionLayout::firstFloor(std::mt19937 &random) {
    Floor floor{};
    constexpr int32_t x = 7;
    constexpr int32_t z = 4;
    fill(floor, x, z, x + 1, z + 1, Cell::Entrance);
    fill(floor, x - 1, z, x - 1, z + 1, Cell::Room);
    fill(floor, x + 2, z - 2, x + 3, z + 3, Cell::Outside);
    fill(floor, x + 1, z - 2, x + 1, z - 1, Cell::Corridor);
    fill(floor, x + 1, z + 2, x + 1, z + 3, Cell::Corridor);
    floor[z - 1][x - 1] = Cell::Corridor;
    floor[z + 2][x - 1] = Cell::Corridor;
    fill(floor, 0, 0, SIZE - 1, 1, Cell::Outside);
    fill(floor, 0, 9, SIZE - 1, SIZE - 1, Cell::Outside);

    carve(floor, random, x, z - 2, West, 6);
    carve(floor, random, x, z + 3, West, 6);
    carve(floor, random, x - 2, z - 1, West, 3);
    carve(floor, random, x - 2, z + 2, West, 3);
    while (fillGaps(floor)) {}
    return floor;
}

MansionLayout::Rooms MansionLayout::assignRooms(const Floor &floor, std::mt19937 &random) {
    std::vector<std::pair<int32_t, int32_t>> positions;
    for (int32_t z = 0; z < SIZE; ++z) {
        for (int32_t x = 0; x < SIZE; ++x) {
            if (floor[z][x] == Cell::Room)
                positions.emplace_back(x, z);
        }
    }
    for (size_t count = positions.size(); count > 1; --count) {
        const size_t last = count - 1;
        std::swap(positions[last], positions[random() % last]);
    }

    Rooms rooms{};
    uint8_t id = 10;
    for (const auto &[x, z] : positions) {
        if (rooms[z][x].mId != 0)
            continue;

        const RoomBounds bounds = pickRoom(floor, rooms, x, z);
        const bool left = (random() & 0x08000000U) != 0;
        const bool north = (random() & 0x08000000U) != 0;
        const int32_t firstX = left ? bounds.x0 : bounds.x1;
        const int32_t firstZ = north ? bounds.z0 : bounds.z1;
        const int32_t otherX = left ? bounds.x1 : bounds.x0;
        const int32_t otherZ = north ? bounds.z1 : bounds.z0;
        const std::array<std::pair<int32_t, int32_t>, 4> corners = {{
                {firstX, firstZ}, {otherX, otherZ}, {otherX, firstZ}, {firstX, otherZ},
        }};
        const auto door = std::find_if(corners.begin(), corners.end(), [&](const auto &corner) {
            return touchesCorridor(floor, corner.first, corner.second);
        });
        const auto [originX, originZ] = door == corners.end() ? std::pair{bounds.x0, bounds.z0} : *door;

        for (int32_t roomZ = bounds.z0; roomZ <= bounds.z1; ++roomZ) {
            for (int32_t roomX = bounds.x0; roomX <= bounds.x1; ++roomX)
                rooms[roomZ][roomX] = {id, bounds.size, false, false};
        }
        rooms[originZ][originX].mOrigin = true;
        rooms[originZ][originX].mHasDoor = door != corners.end();
        ++id;
    }
    return rooms;
}

MansionLayout::Plan MansionLayout::build(std::mt19937 &random) {
    Plan plan{};
    plan.mBase = firstFloor(random);
    plan.mRooms[0] = assignRooms(plan.mBase, random);
    plan.mRooms[1] = assignRooms(plan.mBase, random);

    std::vector<std::pair<int32_t, int32_t>> stairs;
    for (int32_t z = 0; z < SIZE; ++z) {
        for (int32_t x = 0; x < SIZE; ++x) {
            const RoomCell &room = plan.mRooms[1][z][x];
            if (room.mSize == RoomSize::Double && room.mHasDoor)
                stairs.emplace_back(x, z);
            if (!occupied(plan.mBase, x, z))
                plan.mUpper[z][x] = Cell::Outside;
        }
    }
    if (!stairs.empty()) {
        const auto [x, z] = stairs[random() % stairs.size()];
        RoomCell &room = plan.mRooms[1][z][x];
        int32_t landingX = x;
        int32_t landingZ = z;
        constexpr std::array<Direction, 4> LANDING_DIRECTIONS = {North, East, South, West};
        for (Direction direction : LANDING_DIRECTIONS) {
            const int32_t nx = x + DX[direction];
            const int32_t nz = z + DZ[direction];
            if (contains(nx, nz) && plan.mRooms[1][nz][nx].mId == room.mId) {
                landingX = nx;
                landingZ = nz;
                break;
            }
        }
        plan.mUpper[z][x] = Cell::Entrance;
        plan.mUpper[landingZ][landingX] = Cell::Landing;
        std::vector<Direction> exits;
        for (Direction direction : LANDING_DIRECTIONS) {
            if (get(plan.mUpper, landingX + DX[direction], landingZ + DZ[direction]) == Cell::Empty)
                exits.push_back(direction);
        }
        if (!exits.empty()) {
            room.mStairs = true;
            const Direction direction = exits[random() % exits.size()];
            carve(plan.mUpper, random, landingX + DX[direction], landingZ + DZ[direction], direction, 4);
            while (fillGaps(plan.mUpper)) {}
            plan.mRooms[2] = assignRooms(plan.mUpper, random);
            return plan;
        }
    }
    fill(plan.mUpper, 0, 0, SIZE - 1, SIZE - 1, Cell::Outside);
    return plan;
}
