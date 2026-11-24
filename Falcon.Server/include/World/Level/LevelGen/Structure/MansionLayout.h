#pragma once

#include <array>
#include <cstdint>
#include <random>

namespace MansionLayout {
    enum class Cell : uint8_t {
        Empty,
        Corridor,
        Room,
        Entrance,
        Landing,
        Outside
    };

    inline constexpr int32_t SIZE = 11;
    using Floor = std::array<std::array<Cell, SIZE>, SIZE>;

    enum class RoomSize : uint8_t { Single, Double, Square };

    struct RoomCell {
        uint8_t mId = 0;
        RoomSize mSize = RoomSize::Single;
        bool mOrigin = false;
        bool mHasDoor = false;
        bool mStairs = false;
    };

    using Rooms = std::array<std::array<RoomCell, SIZE>, SIZE>;

    struct Plan {
        Floor mBase;
        Floor mUpper;
        std::array<Rooms, 3> mRooms;
    };

    // leaves the random stream at the first room assignment
    Floor firstFloor(std::mt19937 &random);
    Rooms assignRooms(const Floor &floor, std::mt19937 &random);
    Plan build(std::mt19937 &random);
}
