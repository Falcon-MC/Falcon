#include "Block/Blocks/TurtleEggBlock.h"

#include "Block/BlockClassRegistry.h"
#include "Block/Blocks/EggHelpers.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Level/Level.h"
#include "Level/LightSystem.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/PlaySoundPacket.h"

FALCON_REGISTER_BLOCK(TurtleEggBlock, 185);

namespace {
    const char *const EGG_COUNT = "turtle_egg_count";
    const char *const EGG_COUNTS[] = {"one_egg", "two_egg", "three_egg", "four_egg"};
    const int32_t EGG_COUNT_VALUES = 4;
    const int32_t OFF_HOURS_CHANCE = 500;
    const float HATCH_WINDOW_START = 0.65f;
    const float HATCH_WINDOW_END = 0.7f;
    const float CRACK_VOLUME = 0.7f;
    const float BABY_SCALE = 0.16f;
    const char *const TURTLE = "minecraft:turtle";

    bool isSand(const std::string &identifier) {
        return identifier == "minecraft:sand" || identifier == "minecraft:red_sand"
               || identifier == "minecraft:suspicious_sand";
    }

    int32_t eggCount(const BlockState &state) {
        const std::string value = state.mStates.getString(EGG_COUNT, EGG_COUNTS[0]);
        for (int32_t index = 0; index < EGG_COUNT_VALUES; index++) {
            if (value == EGG_COUNTS[index])
                return index + 1;
        }

        return 1;
    }

    Vector3f centerOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }
}

bool TurtleEggBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:turtle_egg";
}

bool TurtleEggBlock::isHatchingTime(Level &level) {
    const float angle = LightSystem::calculateCelestialAngle(level.getTime());
    return angle > HATCH_WINDOW_START && angle < HATCH_WINDOW_END;
}

void TurtleEggBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  const BlockState &state) const {
    const BlockState below = level.getBlockState(position.x, position.y - 1, position.z);
    if (!isSand(below.mName))
        return;

    if (!isHatchingTime(level) && RandomTickSystem::nextInt(OFF_HOURS_CHANCE) != 0)
        return;

    if (EggHelpers::isFullyCracked(state)) {
        hatch(owner, level, position, state);
        return;
    }

    owner.playNamedSound(level, PlaySoundName::TURTLE_EGG_CRACK, centerOf(position), CRACK_VOLUME, EggHelpers::soundPitch());
    level.setBlock(position, EggHelpers::cracked(state), true);
}

void TurtleEggBlock::hatch(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) {
    const int32_t turtles = eggCount(state);
    owner.playNamedSound(level, PlaySoundName::TURTLE_EGG_CRACK, centerOf(position), CRACK_VOLUME, EggHelpers::soundPitch());
    level.setBlock(position, BlockState("minecraft:air"), true);

    for (int32_t turtle = 0; turtle < turtles; turtle++) {
        const Vector3f spawnAt((float) position.x + 0.3f + (float) turtle * 0.2f, (float) position.y,
                               (float) position.z + 0.3f);
        owner.spawnBabyActor(level, TURTLE, spawnAt, BABY_SCALE);
    }
}
