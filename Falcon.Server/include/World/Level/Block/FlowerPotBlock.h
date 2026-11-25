#pragma once

#include "World/Level/Block/Block.h"

#include <string>

class FlowerPotBlockActor;

class FlowerPotBlock final : public Block {
public:
    explicit FlowerPotBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    // the blocks the game gives minecraft:flower_pottable
    static bool isPottable(const std::string &blockIdentifier);

    // the eyeblossom a pot holds at this time: open from 12601 to 23400 in the day, closed otherwise; empty where
    // the dimension has no day
    static std::string eyeblossomFor(const Level &level);

    const char *getBlockActorId() const override;

    // a pottable plant in hand fills an empty pot; anything else takes the plant out
    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    // a creative player breaking it takes nothing out with it
    void onPlayerWillDestroy(ServerNetworkHandler &owner, ServerPlayer &player, Level &level,
                             const Vector3i &position, const BlockState &state) const override;

    // on a block that supports it from above
    bool canSurvive(Level &level, const Vector3i &position, const BlockState &state) const override;

    bool hasRandomTick() const override {
        return true;
    }

    // a potted eyeblossom opens and closes with the time of day
    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    // the plant changed: viewers get the pot's data and the flipped update_bit that redraws it
    static void changed(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                        const FlowerPotBlockActor &pot, Actor *source);
};
