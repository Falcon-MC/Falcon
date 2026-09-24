#include "Block/Blocks/PortalHelpers.h"

#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"

namespace PortalHelpers {
    Vector3f centerOf(const Vector3i &position) {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }

    bool isInsideLevel(Level &level, const Vector3i &position) {
        if (position.y < LevelChunk::MIN_Y || position.y > LevelChunk::MAX_Y)
            return false;

        return position.y >= level.getMinY() && position.y <= level.getMaxY();
    }

    std::string identifierAt(Level &level, int32_t x, int32_t y, int32_t z) {
        if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
            return std::string(AIR_IDENTIFIER);

        return level.getBlockState(x, y, z).mName;
    }

    bool isAirAt(Level &level, int32_t x, int32_t y, int32_t z) {
        return identifierAt(level, x, y, z) == AIR_IDENTIFIER;
    }

    std::string stateText(const BlockState &state, const std::string &key) {
        const Tag *tag = state.mStates.get(key);
        if (tag == nullptr || tag->getType() != Tag::Type::String)
            return std::string();

        return tag->asString();
    }

    BlockState makePortalState(const char *axis) {
        Tag states = Tag::ofCompound();
        states.putString("portal_axis", std::string(axis));
        return BlockState(std::string(PORTAL_IDENTIFIER), states);
    }

    void writeBlock(Level &level, const Vector3i &position, const BlockState &state, ServerNetworkHandler *owner) {
        if (!isInsideLevel(level, position))
            return;

        level.setBlockState(position.x, position.y, position.z, state);

        if (owner != nullptr)
            BlockActionHandler::broadcastBlockUpdate(*owner, level, position, state);
    }
}
