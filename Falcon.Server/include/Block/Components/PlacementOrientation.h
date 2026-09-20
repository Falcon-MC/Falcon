#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cmath>
#include <string>

namespace PlacementOrientation {
    constexpr int FACE_DOWN = 0;
    constexpr int FACE_UP = 1;
    constexpr int FACE_NORTH = 2;
    constexpr int FACE_SOUTH = 3;
    constexpr int FACE_WEST = 4;
    constexpr int FACE_EAST = 5;

    constexpr float PLAYER_EYE_HEIGHT = 1.62f;

    inline int horizontalFacing(float yaw) {
        float angle = std::fmod(yaw, 360.0f);
        if (angle < 0.0f)
            angle += 360.0f;

        if (angle < 45.0f || angle >= 315.0f)
            return FACE_SOUTH;
        if (angle < 135.0f)
            return FACE_WEST;
        if (angle < 225.0f)
            return FACE_NORTH;
        return FACE_EAST;
    }

    inline int horizontalOrdinal(int facing) {
        switch (facing) {
            case FACE_SOUTH:
                return 0;
            case FACE_WEST:
                return 1;
            case FACE_NORTH:
                return 2;
            default:
                return 3;
        }
    }

    inline int ewsnOrdinal(int facing) {
        switch (facing) {
            case FACE_EAST:
                return 0;
            case FACE_WEST:
                return 1;
            case FACE_SOUTH:
                return 2;
            default:
                return 3;
        }
    }

    inline const char *cardinalName(int facing) {
        switch (facing) {
            case FACE_NORTH:
                return "north";
            case FACE_SOUTH:
                return "south";
            case FACE_WEST:
                return "west";
            default:
                return "east";
        }
    }

    inline const char *pillarAxis(int face) {
        if (face == FACE_NORTH || face == FACE_SOUTH)
            return "z";
        if (face == FACE_WEST || face == FACE_EAST)
            return "x";
        return "y";
    }

    inline int faceFromName(const std::string &name) {
        if (name == "down")
            return FACE_DOWN;
        if (name == "up")
            return FACE_UP;
        if (name == "north")
            return FACE_NORTH;
        if (name == "south")
            return FACE_SOUTH;
        if (name == "west")
            return FACE_WEST;
        if (name == "east")
            return FACE_EAST;
        return -1;
    }

    inline const char *faceName(int face) {
        switch (face) {
            case FACE_DOWN:
                return "down";
            case FACE_UP:
                return "up";
            case FACE_NORTH:
                return "north";
            case FACE_SOUTH:
                return "south";
            case FACE_WEST:
                return "west";
            default:
                return "east";
        }
    }

    inline Vector3i faceOffset(int face) {
        switch (face) {
            case FACE_DOWN:
                return Vector3i(0, -1, 0);
            case FACE_UP:
                return Vector3i(0, 1, 0);
            case FACE_NORTH:
                return Vector3i(0, 0, -1);
            case FACE_SOUTH:
                return Vector3i(0, 0, 1);
            case FACE_WEST:
                return Vector3i(-1, 0, 0);
            default:
                return Vector3i(1, 0, 0);
        }
    }

    inline Vector3i relativePosition(const Vector3i &position, int face) {
        const Vector3i offset = faceOffset(face);
        return Vector3i(position.x + offset.x, position.y + offset.y, position.z + offset.z);
    }

    inline int signRotation(float yaw) {
        const float value = ((yaw + 180.0f) * 16.0f / 360.0f) + 0.5f;
        int rotation = (int) std::floor(value) % 16;
        if (rotation < 0)
            rotation += 16;
        return rotation;
    }

    inline const char *leverDirection(int face, int playerFacing) {
        if (face == FACE_DOWN)
            return playerFacing == FACE_WEST || playerFacing == FACE_EAST ? "down_east_west"
                                                                          : "down_north_south";
        if (face == FACE_UP)
            return playerFacing == FACE_WEST || playerFacing == FACE_EAST ? "up_east_west"
                                                                          : "up_north_south";
        return faceName(face);
    }

    inline int faceFromTorchFacing(const std::string &attachment) {
        if (attachment == "south")
            return FACE_NORTH;
        if (attachment == "north")
            return FACE_SOUTH;
        if (attachment == "east")
            return FACE_WEST;
        if (attachment == "west")
            return FACE_EAST;
        return FACE_UP;
    }

    inline const char *torchFacingDirection(int face) {
        switch (face) {
            case FACE_NORTH:
                return "south";
            case FACE_SOUTH:
                return "north";
            case FACE_WEST:
                return "east";
            case FACE_EAST:
                return "west";
            default:
                return "top";
        }
    }

    inline int pistonFacingDirection(int playerFacing, const Vector3f &playerPosition, const Vector3i &blockPosition) {
        const int32_t playerBlockX = (int32_t) std::floor(playerPosition.x);
        const int32_t playerBlockZ = (int32_t) std::floor(playerPosition.z);

        if (std::abs(playerBlockX - blockPosition.x) > 1 || std::abs(playerBlockZ - blockPosition.z) > 1)
            return playerFacing;

        const float eyeY = playerPosition.y + PLAYER_EYE_HEIGHT;
        if (eyeY - (float) blockPosition.y > 2.0f)
            return FACE_UP;
        if ((float) blockPosition.y - eyeY > 0.0f)
            return FACE_DOWN;

        return playerFacing;
    }

    inline std::string crafterOrientation(float pitch, int oppositeFacing) {
        const bool lookingDown = pitch > 45.0f;
        const bool lookingUp = pitch < -45.0f;

        if (lookingDown)
            return std::string("up_") + cardinalName(oppositeFacing);
        if (lookingUp)
            return std::string("down_") + cardinalName(oppositeFacing);

        return std::string(cardinalName(oppositeFacing)) + "_up";
    }
}
