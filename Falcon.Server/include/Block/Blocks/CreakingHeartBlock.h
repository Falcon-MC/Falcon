#pragma once

#include "Block/Block.h"

#include <string>
#include <vector>

class CreakingHeartBlock : public Block {
public:
    explicit CreakingHeartBlock(const Block &block) : Block(block) {
    }

    static bool matches(const std::string &identifier);

    void onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                  const BlockState &state, const ItemStack &usedItem, int blockFace) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const override;

    void onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                  const BlockState &state) const override;

    bool onActorEvent(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state, const std::string &event, MobActor &source) const override;

    bool bindsHomeActors() const override;

private:
    static std::vector<MobActor *> findBoundActors(ServerNetworkHandler &owner, Level &level,
                                                   const Vector3i &position);

    static void scheduleNextUpdate(Level &level, const Vector3i &position);

    static BlockState refreshState(Level &level, const Vector3i &position, const BlockState &state, bool bound);

    static bool hasRequiredLogs(Level &level, const Vector3i &position, const std::string &axis);

    static bool canSpawnCreaking(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 const BlockState &state);

    static bool findSpawnPosition(Level &level, const Vector3i &position, Vector3f &spawn);

    static void spawnCreaking(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const Vector3f &spawn);

    static void spreadResin(Level &level, const Vector3i &position);
};
