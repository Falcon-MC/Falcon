#include "World/Level/Block/BlockPrecipitation.h"

#include "World/Level/Block/BlockData.h"

namespace {
    BlockPrecipitationInteraction interactionOf(const BlockState &state) {
        const BlockData *data = BlockDataTable::find(state.mName.c_str());
        return data == nullptr ? BlockPrecipitationInteraction::ObstructRain : data->mPrecipitation;
    }
}

bool BlockPrecipitation::letsRainThrough(const BlockState &state) {
    return interactionOf(state) != BlockPrecipitationInteraction::ObstructRain;
}

bool BlockPrecipitation::isSnowloggable(const BlockState &state) {
    return interactionOf(state) == BlockPrecipitationInteraction::Snowlogging;
}
