#pragma once

#include <cstdint>

namespace MovementPhysics {
    inline constexpr float FLY_SPEED = 0.05f;
    inline constexpr float VERTICAL_FLY_SPEED = 1.0f;
    inline constexpr float AIR_FRICTION = 0.91f;
    inline constexpr float GRAVITY = 0.08f;
    inline constexpr float SLOW_FALLING_GRAVITY = 0.01f;
    inline constexpr float GRAVITY_MULTIPLIER = 0.98f;
    inline constexpr float STEP_HEIGHT = 0.5625f;
    inline constexpr float SNEAK_INPUT = 0.3f;
    inline constexpr float SWIFT_SNEAK_PER_LEVEL = 0.15f;
    inline constexpr int32_t SWIFT_SNEAK_DELAY_TICKS = 2;
    inline constexpr float JUMP_BOOST_PER_LEVEL = 0.1f;
    inline constexpr float CONSUMING_INPUT = 0.1225f;
    inline constexpr float WALK_AIR_SPEED = 0.02f;
    inline constexpr float SPRINT_AIR_SPEED = 0.026f;
    inline constexpr float SPRINT_SPEED_MULTIPLIER = 1.3f;
    inline constexpr float DEFAULT_BLOCK_FRICTION = 0.6f;
    inline constexpr float IMPULSE_SCALE = 0.98f;
    inline constexpr float PLAYER_WIDTH = 0.6f;
    inline constexpr float PLAYER_HEIGHT = 1.8f;
    inline constexpr float PLAYER_SNEAKING_HEIGHT = 1.49f;
    inline constexpr float PLAYER_CRAWLING_HEIGHT = 0.6f;
    inline constexpr int32_t JUMP_DELAY_TICKS = 10;
    inline constexpr float EDGE_INSET = 0.025f;
    inline constexpr float EDGE_STEP = 0.05f;
    inline constexpr int32_t EDGE_MAX_ITERATIONS = 1000;
    inline constexpr float SUPPORT_PROBE_DEPTH = 0.2f;
    inline constexpr float SUPPORT_PROBE_MARGIN = 1.0e-3f;
    inline constexpr float MIN_BOUNCE_VELOCITY = 1.0e-4f;
    inline constexpr float WALK_SLOWDOWN_LIMIT = 0.1f;
    inline constexpr float WALK_SLOWDOWN_BASE = 0.4f;
    inline constexpr float WALK_SLOWDOWN_PER_VELOCITY = 0.2f;
    inline constexpr float GROUND_PROBE_DEPTH = 0.1f;
    inline constexpr float JUMP_PROBE_DEPTH = 0.1f;
    inline constexpr float JUMP_VELOCITY = 0.42f;
    inline constexpr float CLIMB_SPEED = 0.2f;
    inline constexpr float TRAVERSAL_SPEED = 0.15f;
    inline constexpr float POWDER_SNOW_ASCEND_SPEED = 0.2f;
    inline constexpr float HONEY_CONTACT_MARGIN = 1.0e-3f;
    inline constexpr float HONEY_HORIZONTAL_SLIDE = 0.4f;
    inline constexpr float HONEY_MAX_FALL = -0.12f;
    inline constexpr const char *const LEATHER_BOOTS = "minecraft:leather_boots";
    inline constexpr float MOJANG_PI = 3.1415927f;
    inline constexpr float SIN_COEFFICIENTS[] = {
            1.5896230e-10f, -2.5050748e-8f, 2.7557314e-6f, -1.9841270e-4f, 8.333334e-3f, -1.6666667e-1f
    };
    inline constexpr float COS_COEFFICIENTS[] = {
            -1.1358537e-11f, 2.0875701e-9f, -2.7557314e-7f, 2.4801588e-5f, -1.3888889e-3f, 4.1666668e-2f
    };

}
