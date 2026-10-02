#pragma once

#include "Actor/MobEffect.h"

#include <cstdint>
#include <string>
#include <vector>

struct PotionEffect {
    MobEffectId mId;
    int32_t mDuration;
    int32_t mAmplifier;
    bool mInstant;
};

const int32_t ACTOR_DATA_POTION_COLOR = 8;
const int32_t ACTOR_DATA_POTION_AUX_VALUE = 36;
const int32_t ACTOR_DATA_AREA_EFFECT_CLOUD_RADIUS = 61;
const int32_t ACTOR_DATA_AREA_EFFECT_CLOUD_PARTICLE_ID = 63;
const int32_t ACTOR_DATA_WIDTH = 53;
const int32_t ACTOR_DATA_HEIGHT = 54;

const int32_t AREA_EFFECT_CLOUD_POTION_PARTICLE = 32;

std::vector<PotionEffect> getPotionEffects(int32_t potionId);

int32_t getPotionColor(int32_t potionId);

int32_t findPotionId(const std::string &name);
