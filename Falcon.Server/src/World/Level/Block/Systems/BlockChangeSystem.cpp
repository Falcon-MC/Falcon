#include "World/Level/Block/Systems/BlockChangeSystem.h"

#include "World/Level/Level.h"
#include "Falcon/Plugin/PluginEvent.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Falcon/Plugin/PluginWorldEventData.h"
#include "Core/Json/Json.h"

#include <cmath>

namespace {
    FalconEventType eventTypeOf(BlockChangeCause cause) {
        switch (cause) {
            case BlockChangeCause::Grow:
                return FALCON_EVENT_BLOCK_GROW;
            case BlockChangeCause::Spread:
                return FALCON_EVENT_BLOCK_SPREAD;
            case BlockChangeCause::Form:
                return FALCON_EVENT_BLOCK_FORM;
            case BlockChangeCause::Fade:
                return FALCON_EVENT_BLOCK_FADE;
            case BlockChangeCause::Decay:
                return FALCON_EVENT_LEAVES_DECAY;
        }
        return FALCON_EVENT_BLOCK_GROW;
    }
}

bool BlockChangeSystem::allows(Level &level, const Vector3i &position, const BlockState &state, BlockChangeCause cause,
                               const Vector3i *source, ServerPlayer *player) {
    const FalconEventType type = eventTypeOf(cause);
    PluginManager *plugins = PluginManager::findWithSubscribers(type);
    if (plugins == nullptr)
        return true;

    const BlockState *current = level.peekBlockPtr(position.x, position.y, position.z);

    PluginEvent event;
    event.mType = type;
    event.mCancellable = true;
    event.mLevel = &level;
    event.mPlayer = player;
    event.mBlockPosition = position;
    event.mBlockName = state.mName;
    event.mPreviousBlockName = current == nullptr ? std::string() : current->mName;
    event.mCustomData = PluginWorldEventData::encodeBlock(state);
    if (source != nullptr)
        event.mFrom = Vector3f((float) source->x, (float) source->y, (float) source->z);
    plugins->dispatch(event);
    return !event.mCancelled;
}

bool BlockChangeSystem::change(Level &level, const Vector3i &position, const BlockState &state, BlockChangeCause cause,
                               bool update, ServerPlayer *player) {
    if (!allows(level, position, state, cause, nullptr, player))
        return false;

    level.setBlock(position, state, update);
    return true;
}

bool BlockChangeSystem::prepareStructure(Level &level, const Vector3i &origin, StructureBlocks &blocks,
                                         ServerPlayer *player) {
    PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_STRUCTURE_GROW);
    if (plugins == nullptr)
        return true;

    std::string data = "[";
    for (const auto &[position, state]: blocks) {
        if (data.size() > 1)
            data += ',';
        data += '[' + std::to_string(position.x) + ',' + std::to_string(position.y) + ','
                + std::to_string(position.z) + ',' + PluginWorldEventData::encodeBlock(state) + ']';
    }
    data += ']';

    PluginEvent event;
    event.mType = FALCON_EVENT_STRUCTURE_GROW;
    event.mCancellable = true;
    event.mLevel = &level;
    event.mPlayer = player;
    event.mBlockPosition = origin;
    event.mCustomData = data;
    plugins->dispatch(event);
    if (event.mCancelled)
        return false;
    if (event.mCustomData == data)
        return !blocks.empty();

    const auto changed = json::parse(event.mCustomData);
    if (changed == nullptr || !changed->isArray())
        return false;
    StructureBlocks replacements;
    replacements.reserve(changed->mArray.size());
    for (const auto &entry: changed->mArray) {
        if (!entry->isArray() || entry->mArray.size() != 4)
            return false;
        const auto &fields = entry->mArray;
        if (!fields[0]->isNumber() || !fields[1]->isNumber() || !fields[2]->isNumber())
            return false;
        const double x = fields[0]->number();
        const double y = fields[1]->number();
        const double z = fields[2]->number();
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || std::abs(x) > 30'000'000
            || std::abs(z) > 30'000'000 || y < level.getMinY() || y > level.getMaxY())
            return false;
        const Vector3i position((int32_t) x, (int32_t) y, (int32_t) z);
        BlockState state;
        if (!level.isChunkResident(position.x >> 4, position.z >> 4)
            || !PluginWorldEventData::decodeBlock(*fields[3], state))
            return false;
        replacements.emplace_back(position, std::move(state));
    }
    blocks = std::move(replacements);
    return !blocks.empty();
}

void BlockChangeSystem::applyStructure(Level &level, const StructureBlocks &blocks, bool update) {
    for (const auto &[position, state]: blocks)
        level.setBlock(position, state, update);
}

bool BlockChangeSystem::allowsFlow(Level &level, const Vector3i &source, const Vector3i &position,
                                   const BlockState &liquid) {
    PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_LIQUID_FLOW);
    if (plugins == nullptr)
        return true;

    PluginEvent event;
    event.mType = FALCON_EVENT_LIQUID_FLOW;
    event.mCancellable = true;
    event.mLevel = &level;
    event.mBlockPosition = position;
    event.mBlockName = liquid.mName;
    event.mFrom = Vector3f((float) source.x, (float) source.y, (float) source.z);
    plugins->dispatch(event);
    return !event.mCancelled;
}

bool BlockChangeSystem::spread(Level &level, const Vector3i &source, const Vector3i &position, const BlockState &state,
                               bool update) {
    if (!allows(level, position, state, BlockChangeCause::Spread, &source))
        return false;

    level.setBlock(position, state, update);
    return true;
}
