#include "World/Level/Block/Systems/BlockChangeSystem.h"

#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

namespace {
    EventSignal<BlockChangeBeforeEvent> &signalOf(BeforeEvents &before, BlockChangeCause cause) {
        switch (cause) {
            case BlockChangeCause::Grow:
                return before.mBlockGrow;
            case BlockChangeCause::Spread:
                return before.mBlockSpread;
            case BlockChangeCause::Form:
                return before.mBlockForm;
            case BlockChangeCause::Fade:
                return before.mBlockFade;
            case BlockChangeCause::Decay:
                return before.mLeavesDecay;
        }
        return before.mBlockGrow;
    }
}

bool BlockChangeSystem::allows(Level &level, const Vector3i &position, const BlockState &state, BlockChangeCause cause,
                               const Vector3i *source, ServerPlayer *player) {
    ServerNetworkHandler *owner = level.getOwner();
    if (owner == nullptr)
        return true;
    EventSignal<BlockChangeBeforeEvent> &signal = signalOf(owner->getEventBus().before(), cause);
    if (!signal.hasSubscribers())
        return true;

    BlockChangeBeforeEvent event(level, position, state, level.peekBlockPtr(position.x, position.y, position.z), source,
                                 player);
    signal.emit(event);
    return !event.isCancelled();
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
    ServerNetworkHandler *owner = level.getOwner();
    if (owner == nullptr)
        return true;
    EventSignal<StructureGrowBeforeEvent> &structureGrow = owner->getEventBus().before().mStructureGrow;
    if (!structureGrow.hasSubscribers())
        return true;

    StructureGrowBeforeEvent event(level, origin, blocks, player);
    structureGrow.emit(event);
    if (event.isCancelled())
        return false;
    return !blocks.empty();
}

void BlockChangeSystem::applyStructure(Level &level, const StructureBlocks &blocks, bool update) {
    for (const auto &[position, state]: blocks)
        level.setBlock(position, state, update);
}

bool BlockChangeSystem::allowsFlow(Level &level, const Vector3i &source, const Vector3i &position,
                                   const BlockState &liquid) {
    ServerNetworkHandler *owner = level.getOwner();
    if (owner == nullptr)
        return true;
    EventSignal<LiquidFlowBeforeEvent> &liquidFlow = owner->getEventBus().before().mLiquidFlow;
    if (!liquidFlow.hasSubscribers())
        return true;

    LiquidFlowBeforeEvent event(level, source, position, liquid);
    liquidFlow.emit(event);
    return !event.isCancelled();
}

bool BlockChangeSystem::spread(Level &level, const Vector3i &source, const Vector3i &position, const BlockState &state,
                               bool update) {
    if (!allows(level, position, state, BlockChangeCause::Spread, &source))
        return false;

    level.setBlock(position, state, update);
    return true;
}
