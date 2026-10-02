#include "Block/Systems/BlockChangeSystem.h"

#include "Level/Level.h"
#include "Plugin/PluginEvent.h"
#include "Plugin/PluginManager.h"

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

bool BlockChangeSystem::allows(Level &level, const Vector3i &position, const BlockState &state,
                               BlockChangeCause cause, const Vector3i *source) {
    const FalconEventType type = eventTypeOf(cause);
    PluginManager *plugins = PluginManager::findWithSubscribers(type);
    if (plugins == nullptr)
        return true;

    const BlockState *current = level.peekBlockPtr(position.x, position.y, position.z);

    PluginEvent event;
    event.mType = type;
    event.mCancellable = true;
    event.mLevel = &level;
    event.mBlockPosition = position;
    event.mBlockName = state.mName;
    event.mPreviousBlockName = current == nullptr ? std::string() : current->mName;
    if (source != nullptr)
        event.mFrom = Vector3f((float) source->x, (float) source->y, (float) source->z);
    plugins->dispatch(event);
    return !event.mCancelled;
}

bool BlockChangeSystem::change(Level &level, const Vector3i &position, const BlockState &state,
                               BlockChangeCause cause, bool update) {
    if (!allows(level, position, state, cause))
        return false;

    level.setBlock(position, state, update);
    return true;
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

bool BlockChangeSystem::spread(Level &level, const Vector3i &source, const Vector3i &position,
                               const BlockState &state, bool update) {
    if (!allows(level, position, state, BlockChangeCause::Spread, &source))
        return false;

    level.setBlock(position, state, update);
    return true;
}
