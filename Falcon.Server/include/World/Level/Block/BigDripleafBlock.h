#pragma once

#include "World/Level/Block/Block.h"

#include <cstdint>
#include <string>

// a column of stems under one leaf; the leaf tilts under a standing actor or a projectile and springs back
class BigDripleafBlock : public Block {
public:
    enum class Tilt : uint8_t { None, Unstable, Partial, Full };

    static constexpr const char *IDENTIFIER = "minecraft:big_dripleaf";

    // the leaf tips once an actor stands above this height in the block
    static constexpr float TILT_HEIGHT = 0.6875f;

    static constexpr int32_t UNSTABLE_TICKS = 10;
    static constexpr int32_t PARTIAL_TICKS = 10;
    static constexpr int32_t FULL_TICKS = 100;

    explicit BigDripleafBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    static Tilt getTilt(const BlockState &state);

    static bool isHead(const BlockState &state);

    static BlockState withParts(const BlockState &state, bool head, Tilt tilt);

    // a column of 2 to 5 blocks where a small dripleaf stood, as far as the space above allows
    static void growFromSmall(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const std::string &direction);

    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;

    bool watchesNeighbour(const Vector3i &position, const BlockState &state, const Vector3i *changed) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;

    void onPlacedInLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                         const BlockState &state) const override;

    void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const override;

    void onRedstonePower(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                         const BlockState &state) const override;

    void onStepOn(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                  const BlockState &state) const override;

    bool onProjectileHit(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state,
                         ServerActor &projectile) const override;

    bool getCollisionShape(const BlockState &state, const Actor *actor, AxisAlignedBB &shape) const override;

    bool onFertilized(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

private:
    static bool canGrowInto(const BlockState &state);

    static void setTilt(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state,
                        Tilt tilt);

    static void resetTilt(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                          const BlockState &state);
};
