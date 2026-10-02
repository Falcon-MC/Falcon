#include "Block/Blocks/EggHelpers.h"

#include "Block/Systems/RandomTickSystem.h"

namespace {
    const char *const NO_CRACKS = "no_cracks";
    const char *const CRACKED = "cracked";
    const char *const MAX_CRACKED = "max_cracked";
    const int32_t PITCH_PRECISION = 1000;
    const float MIN_PITCH = 0.9f;
    const float PITCH_SPREAD = 0.2f;
}

bool EggHelpers::isFullyCracked(const BlockState &state) {
    return state.mStates.getString(CRACKED_STATE, NO_CRACKS) == MAX_CRACKED;
}

BlockState EggHelpers::cracked(const BlockState &state) {
    const std::string current = state.mStates.getString(CRACKED_STATE, NO_CRACKS);
    Tag states = state.mStates;
    states.putString(CRACKED_STATE, current == NO_CRACKS ? CRACKED : MAX_CRACKED);
    return BlockState(state.mName, states);
}

float EggHelpers::soundPitch() {
    return MIN_PITCH + PITCH_SPREAD * (float) RandomTickSystem::nextInt(PITCH_PRECISION) / (float) PITCH_PRECISION;
}
