#pragma once

#include "World/Level/Block/ContainerBlock.h"

#include <string>

class LecternBlock final : public ContainerBlock {
public:
    explicit LecternBlock(const Block &block) : ContainerBlock(block) {}

    static bool matches(const std::string &identifier);

    // LecternBlock::use (sub_143C86CD0): a book in hand goes on an empty lectern, else the book opens
    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    // sub_143C878E0, sub_143C87690: hitting a lectern drops its book; in creative the lectern stays
    bool onPunch(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                 const BlockState &state) const override;

    // sub_143C86590: powered_bit flips; on, it asks for the tick that turns it off
    void onScheduledUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                           const BlockState &state) const override;

    // sub_143C87AE0
    int getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const override;

    bool isSignalSource() const override {
        return true;
    }

    // sub_143C87C20: a page turn asks for the tick that powers the lectern
    static void pulse(Level &level, const Vector3i &position);
};
