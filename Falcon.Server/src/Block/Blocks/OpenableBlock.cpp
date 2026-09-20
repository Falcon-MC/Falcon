#include "Block/Blocks/OpenableBlock.h"

#include "Block/Systems/RedstoneSystem.h"
#include "Level/Dimension.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <array>
#include <unordered_set>

namespace {
    const char *SOUND_DOOR_OPEN = "random.door_open";
    const char *SOUND_DOOR_CLOSE = "random.door_close";

    std::array<std::unordered_set<int64_t>, Dimension::DIMENSION_COUNT> gManualOverrides;

    std::unordered_set<int64_t> &manualOverridesOf(Level &level) {
        return gManualOverrides[level.getDimensionId()];
    }
}

bool OpenableBlock::isOpen(const BlockState &state) {
    const Tag *open = state.mStates.get("open_bit");
    return open != nullptr && open->getType() == Tag::Type::Byte && open->asByte() != 0;
}

void OpenableBlock::setOpen(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state, bool open) {
    Tag states = state.mStates;
    states.putByte("open_bit", open ? 1 : 0);
    level.setBlock(position, BlockState(state.mName, states), false);

    const Vector3f center((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    owner.playLevelSound(level, open ? SOUND_DOOR_OPEN : SOUND_DOOR_CLOSE, center, "", state.getHash());
}

bool OpenableBlock::hasManualOverride(Level &level, const Vector3i &position) {
    return manualOverridesOf(level).count(RedstoneSystem::packPosition(position)) != 0;
}

void OpenableBlock::setManualOverride(Level &level, const Vector3i &position, bool manual) {
    if (manual)
        manualOverridesOf(level).insert(RedstoneSystem::packPosition(position));
    else
        manualOverridesOf(level).erase(RedstoneSystem::packPosition(position));
}

bool OpenableBlock::toggle(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) {
    const bool open = !isOpen(state);
    setOpen(owner, level, position, state, open);
    setManualOverride(level, position, open || RedstoneSystem::isGettingPower(owner, level, position));

    return true;
}

void OpenableBlock::openOnPlace(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                const BlockState &state) {
    if (isOpen(state) || !RedstoneSystem::isGettingPower(owner, level, position))
        return;

    setOpen(owner, level, position, state, true);
}

void OpenableBlock::onRedstoneUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                     const BlockState &state) {
    const bool manualOverride = hasManualOverride(level, position);
    const bool gettingPower = RedstoneSystem::isGettingPower(owner, level, position);
    const bool open = isOpen(state);

    if (open != gettingPower && !manualOverride) {
        setOpen(owner, level, position, state, gettingPower);
        return;
    }

    if (manualOverride && gettingPower == open)
        setManualOverride(level, position, false);
}
