#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class CauldronBlock final : public Block {
public:
    enum class Liquid { Empty, Water, Lava, PowderSnow };

    explicit CauldronBlock(const BlockState &state) : Block(state) {}

    explicit CauldronBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    const char *getBlockActorId() const override;

    int getFillLevel() const;

    Liquid getLiquid() const;

    bool isFull() const;

    bool canFill(Liquid liquid) const;

    BlockState withFillLevel(Liquid liquid, int fillLevel) const;

    static BlockState makeState(Liquid liquid, int fillLevel);

    int getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const override;

    int getLightEmission(const BlockState &state, int dataEmission) const override;

    // buckets, bottles, potions, tipped arrows, dyes and washing; any item, when water sits above
    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    bool hearsActorsInside() const override {
        return true;
    }

    void onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                       const BlockState &state) const override;

    void onPlacedInLevel(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                         const BlockState &state) const override;

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;

    // water above fills it; else a stalactite tip above drips its fluid in
    void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const override;

    // whether a stalactite dripping `fluid` (water or lava) adds to the cauldron there
    static bool canReceiveDrip(Level &level, const Vector3i &position, Liquid fluid);
};
