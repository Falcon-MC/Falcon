#include "World/Level/Block/DetectorRailBlock.h"

#include "World/Actor/ServerActor.h"
#include "World/Inventory/Container.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

// before BaseRailBlock, which matches every rail
FALCON_REGISTER_BLOCK(DetectorRailBlock, 139);

namespace {
    const char *STATE_POWERED = "rail_data_bit";

    bool isPowered(const BlockState &state) {
        return state.mStates.getByte(STATE_POWERED, 0) != 0;
    }

    // minecarts touching (x+0.2, y, z+0.2)-(x+0.8, y+0.8, z+0.8), as DetectorRailBlock fetches them (sub_149C54970)
    template <typename Visitor>
    void forEachMinecart(ServerNetworkHandler &owner, Level &level, const Vector3i &position, Visitor visit) {
        const float minX = (float) position.x + 0.2f;
        const float minY = (float) position.y;
        const float minZ = (float) position.z + 0.2f;
        const float maxX = (float) position.x + 0.8f;
        const float maxY = (float) position.y + 0.8f;
        const float maxZ = (float) position.z + 0.8f;

        for (auto &entry: owner.getActors()) {
            ServerActor *actor = entry.second.get();
            if (actor == nullptr || !actor->isMinecart() || actor->isDead() || !actor->isIn(level))
                continue;

            const Vector3f &feet = actor->getPosition();
            const ActorSize size = actor->getSize();
            const float half = size.mWidth * 0.5f;
            if (feet.x - half < maxX && feet.x + half > minX && feet.y < maxY && feet.y + size.mHeight > minY
                && feet.z - half < maxZ && feet.z + half > minZ && visit(*actor))
                return;
        }
    }
}

bool DetectorRailBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::DETECTOR_RAIL;
}

// a producer of 15 while a minecart is on it, attached downward so it charges the block below (sub_149C552D0)
int DetectorRailBlock::getEmittedSignal(Level &level, const Vector3i &position, const BlockState &state, int face,
                                        bool strong) const {
    (void) level;
    (void) position;
    if (!isPowered(state))
        return 0;

    return !strong || face == RedstoneFace::UP ? RedstoneSystem::MAX_SIGNAL : 0;
}

// DetectorRailBlock::entityInside (sub_149C547F0, 26.60): only a minecart on an unpowered rail checks
void DetectorRailBlock::onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                                      const BlockState &state) const {
    if (!actor.isMinecart() || isPowered(state))
        return;

    checkPressed(owner, owner.getLevelFor(actor), position, state);
}

// DetectorRailBlock tick (sub_149C54F20, 26.60): a powered rail checks again until the minecart leaves
void DetectorRailBlock::onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                          const BlockState &state) const {
    if (isPowered(state))
        checkPressed(owner, level, position, state);
}

// DetectorRailBlock::getComparatorSignal (sub_149C550D0, 26.60): the first minecart with a container on the rail
int DetectorRailBlock::getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) state;
    ServerNetworkHandler *owner = level.getOwner();
    if (owner == nullptr)
        return 0;

    int signal = 0;
    forEachMinecart(*owner, level, position, [&signal](ServerActor &minecart) {
        const Container *container = minecart.getEntityContainer();
        if (container == nullptr)
            return false;

        signal = containerSignal(*container);
        return true;
    });
    return signal;
}

// DetectorRailBlock::_updateState (sub_149C54970, 26.60): rail_data_bit follows the minecarts, then rechecked in 20
void DetectorRailBlock::checkPressed(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                     const BlockState &state) {
    bool pressed = false;
    forEachMinecart(owner, level, position, [&pressed](ServerActor &) {
        pressed = true;
        return true;
    });

    const bool powered = isPowered(state);
    if (pressed != powered) {
        Tag states = state.mStates;
        states.putByte(STATE_POWERED, pressed ? 1 : 0);
        level.setBlock(position, BlockState(state.mName, states), true);
    }

    if (pressed)
        level.scheduleUpdate(position, CHECK_DELAY);

    if (pressed == powered)
        return;

    RedstoneSystem::updateAroundRedstone(owner, level, position);
    RedstoneSystem::updateAroundRedstone(owner, level, RedstoneFace::relative(position, RedstoneFace::DOWN));
}
