#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

#include <cstdint>

class Container;
class ItemStack;
class Level;
class ServerNetworkHandler;

// what the dispenser hands the item's and the block's dispense
struct DispenseSource {
    // LevelEvent 1002 and its data, as every projectile dispense sends them
    static constexpr int32_t LEVEL_EVENT_SHOOT = 1002;
    static constexpr int32_t SHOOT_EVENT_DATA = 78642;

    ServerNetworkHandler &mOwner;
    Level &mLevel;
    Vector3i mBlockPosition;
    Container &mContainer;
    int32_t mSlot;
    // the block center pushed 0.7 out of the facing side, 0.3 above the bottom
    Vector3f mPosition;
    // facing_direction: down, up, north, south, west, east
    int32_t mFace;

    const ItemStack &getItem() const;

    Vector3f getFacingVector() const;

    void removeOne() const;

    // with SHOOT_EVENT_DATA, at the dispense position
    void broadcastShootEvent(int32_t eventId) const;

    // block.click at the dispense position
    void playClick() const;
};
