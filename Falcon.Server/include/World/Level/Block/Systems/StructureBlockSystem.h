#pragma once

#include "World/Level/Structure/McStructure.h"

#include <memory>
#include <vector>

class ScriptStructureRegistry;
class ServerPlayer;
class StructureBlockActor;
class StructureBlockUpdatePacket;
class StructureTemplateDataRequestPacket;

class StructureBlockSystem {
public:
    StructureBlockSystem();
    ~StructureBlockSystem();

    static bool canEdit(ServerPlayer &player, const Vector3i &position);

    void update(ServerNetworkHandler &owner, ServerPlayer &player, const StructureBlockUpdatePacket &packet);

    void request(ServerNetworkHandler &owner, ServerPlayer &player, const StructureTemplateDataRequestPacket &packet);

    void activate(ServerNetworkHandler &owner, Level &level, const StructureBlockActor &actor, bool saveToDisk,
                  bool waterlogged = false);

    void tick(ServerNetworkHandler &owner, Level &level);

private:
    struct Placement {
        Level *mLevel;
        std::shared_ptr<McStructure> mStructure;
        McStructure::PlaceOptions mOptions;
        Vector3i mPosition;
        int64_t mTicks;
        int64_t mTick;
        int64_t mUnits;
        int64_t mCellsPerUnit;
        std::vector<Vector3i> mPendingBlockUpdates;
    };

    ScriptStructureRegistry &_registry(ServerNetworkHandler &owner, Level &level);
    std::shared_ptr<McStructure> _find(ServerNetworkHandler &owner, Level &level, const std::string &name);

    std::unique_ptr<ScriptStructureRegistry> mRegistry;
    std::vector<Placement> mPending;
};
