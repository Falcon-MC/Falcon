#pragma once

#include "Block/BlockActor.h"
#include "Block/Inventory/SimpleContainerInventory.h"

class ContainerBlockActor : public BlockActor {
public:
    explicit ContainerBlockActor(int size) : mInventory(size) {}

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    Container *getContainer() override { return &mInventory; }

    SimpleContainerInventory &getInventory() { return mInventory; }

    const SimpleContainerInventory &getInventory() const { return mInventory; }

    void addViewer() { ++mViewers; }

    void removeViewer() {
        if (mViewers > 0)
            --mViewers;
    }

    int getViewerCount() const { return mViewers; }

protected:
    SimpleContainerInventory mInventory;

private:
    int mViewers = 0;
};

class BarrelBlockActor final : public ContainerBlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Barrel";

    static const int SIZE = 27;

    BarrelBlockActor() : ContainerBlockActor(SIZE) {}

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }
};

class ShulkerBoxBlockActor final : public ContainerBlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "ShulkerBox";

    static const int SIZE = 27;

    ShulkerBoxBlockActor() : ContainerBlockActor(SIZE) {}

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    Tag getSpawnCompound() const override;

    void setFacing(int8_t facing) { mFacing = facing; }

    int8_t getFacing() const { return mFacing; }

private:
    int8_t mFacing = 1;
};

class DispenserBlockActor final : public ContainerBlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Dispenser";

    static const int SIZE = 9;

    DispenserBlockActor() : ContainerBlockActor(SIZE) {}

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }
};

class DropperBlockActor final : public ContainerBlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Dropper";

    static const int SIZE = 9;

    DropperBlockActor() : ContainerBlockActor(SIZE) {}

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }
};

class BrewingStandBlockActor final : public ContainerBlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "BrewingStand";

    static const int SIZE = 5;

    static const int SLOT_INGREDIENT = 0;
    static const int SLOT_FUEL = 4;
    static const int FIRST_POTION_SLOT = 1;
    static const int POTION_SLOTS = 3;
    static const int MAX_BREW_TIME = 400;
    static const int FUEL_PER_BLAZE_POWDER = 20;

    BrewingStandBlockActor() : ContainerBlockActor(SIZE) {}

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    Tag getSpawnCompound() const override;

    bool tick(ServerNetworkHandler &owner) override;

private:
    bool _hasMix() const;

    void _restockFuel(ServerNetworkHandler &owner);

    void _brew(ServerNetworkHandler &owner);

    void _stopBrewing(ServerNetworkHandler &owner);

    void _sendProperty(ServerNetworkHandler &owner, int property, int value) const;

    void _updateSlotStates(ServerNetworkHandler &owner);

    int mBrewTime = MAX_BREW_TIME;
    int mFuelAmount = 0;
    int mFuelTotal = 0;
};

class BeaconBlockActor final : public ContainerBlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Beacon";

    static const int SIZE = 1;

    BeaconBlockActor() : ContainerBlockActor(SIZE) {}

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    int mPrimaryEffect = 0;
    int mSecondaryEffect = 0;
};

class CrafterBlockActor final : public ContainerBlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Crafter";

    static const int SIZE = 9;

    CrafterBlockActor() : ContainerBlockActor(SIZE) {}

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }
};

class CampfireBlockActor final : public ContainerBlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Campfire";

    static const int SIZE = 4;
    static const int COOK_TIME = 600;

    CampfireBlockActor() : ContainerBlockActor(SIZE) {}

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    Tag getSpawnCompound() const override;

    bool tick(ServerNetworkHandler &owner) override;

    bool addFood(const ItemStack &item);

private:
    bool _isSoul() const;

    int mCookTimes[SIZE] = {};
};

class ShelfBlockActor final : public ContainerBlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Shelf";

    static const int SIZE = 3;

    ShelfBlockActor() : ContainerBlockActor(SIZE) {}

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }
};

class ChiseledBookshelfBlockActor final : public ContainerBlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "ChiseledBookshelf";

    static const int SIZE = 6;

    ChiseledBookshelfBlockActor() : ContainerBlockActor(SIZE) {}

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }
};

class DecoratedPotBlockActor final : public ContainerBlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "DecoratedPot";

    static const int SIZE = 1;

    DecoratedPotBlockActor() : ContainerBlockActor(SIZE) {}

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }
};

class JukeboxBlockActor final : public ContainerBlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Jukebox";

    static const int SIZE = 1;

    JukeboxBlockActor() : ContainerBlockActor(SIZE) {}

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }
};

class LecternBlockActor final : public ContainerBlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "Lectern";

    static const int SIZE = 1;

    LecternBlockActor() : ContainerBlockActor(SIZE) {}

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data, const PacketCodecContext &context) override;

    int mPage = 0;
    int mTotalPages = 0;
};

class EnchantTableBlockActor final : public ContainerBlockActor {
public:
    static constexpr const char *BLOCK_ACTOR_ID = "EnchantTable";

    static const int SIZE = 2;

    EnchantTableBlockActor() : ContainerBlockActor(SIZE) {}

    const char *getBlockActorId() const override { return BLOCK_ACTOR_ID; }
};
