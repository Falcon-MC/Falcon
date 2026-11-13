#pragma once

#include "Core/Math/Vector3i.h"
#include "World/Inventory/CartographyRecipes.h"
#include "World/Inventory/Container.h"
#include "Protocol/Types/ContainerType.h"
#include "Protocol/Types/ItemStack.h"

#include <string>

#include <cstdint>
#include <array>
#include <utility>
#include <vector>

class NetworkIdentifier;
class PacketSender;
class ServerPlayer;
class ServerNetworkHandler;
class BlockActorStore;
class ChestBlockActor;
class FurnaceBlockActor;
class Level;
class ContainerBlockActor;
class ServerActor;
enum class FurnaceKind : uint8_t;

class InventoryManager {
public:
    enum class InventoryId : int {
        Inventory = 0,
        Offhand = 1,
        Armor = 2,
        Cursor = 3,
        CraftingInput = 4,
        CraftingTableInput = 5,
        FurnaceInput = 6
    };

    static const int CONTAINER_ID_NONE = -1;
    static const int CONTAINER_ID_INVENTORY = 0;
    static const int CONTAINER_ID_FIRST = 1;
    static const int CONTAINER_ID_LAST = 100;
    static const int CONTAINER_ID_OFFHAND = 119;
    static const int CONTAINER_ID_ARMOR = 120;
    static const int CONTAINER_ID_UI = 124;
    static constexpr int CONTAINER_ID_REGISTRY = 125;

    static const int UI_SLOT_CURSOR = 0;

    InventoryManager();

    void attach(ServerPlayer *player, ServerNetworkHandler *owner);

    void syncAll();

    void syncContents(InventoryId inventory);

    void syncSlot(InventoryId inventory, int slot);

    void syncBundle(const ItemStack &bundle, int32_t bundleId, const std::vector<ItemStack> &contents);

    void syncBundles();

    void syncSelectedHotbarSlot();

    void onClientSelectHotbarSlot(int slot);

    void onClientOpenMainInventory();

    bool onClientOpenCraftingTable(const Vector3i &position);

    bool onClientOpenFurnace(const Vector3i &position, FurnaceKind kind, ContainerType type);

    bool onClientOpenAnvil(const Vector3i &position);

    bool isAnvilOpen() const;

    bool onClientOpenCartography(const Vector3i &position);

    bool isCartographyOpen() const;

    bool onClientOpenStonecutter(const Vector3i &position);

    bool isStonecutterOpen() const;

    int getStonecutterWindowId() const {
        return isStonecutterOpen() ? mAnvilWindowId : CONTAINER_ID_NONE;
    }

    // index into StonecutterRecipes::validRecipes of the input
    int32_t getStonecutterSelectedRecipe() const {
        return mStonecutterSelectedRecipe;
    }

    // the server's choice: the client is told
    void setStonecutterSelectedRecipe(int32_t index);

    void onClientSelectStonecutterRecipe(int containerId, int32_t index);

    void syncStonecutterInput();

    // the copy id a MapCreateLockedCopyPacket asked for, kept until the table locks a map
    void setClientMapLock(const CartographyLock &lock) {
        mClientMapLock = lock;
    }

    const CartographyLock &getClientMapLock() const {
        return mClientMapLock;
    }

    const Vector3i &getAnvilPosition() const;

    bool openContainer(ContainerType type, const Vector3i &position);

    bool isCraftingTableOpen() const {
        return mCraftingTableWindowId != CONTAINER_ID_NONE;
    }

    bool isFurnaceOpen() const {
        return mFurnaceWindowId != CONTAINER_ID_NONE;
    }

    int getCraftingTableWindowId() const {
        return mCraftingTableWindowId;
    }

    int getFurnaceWindowId() const {
        return mFurnaceWindowId;
    }

    const Vector3i &getFurnacePosition() const {
        return mFurnacePosition;
    }

    FurnaceKind getFurnaceKind() const {
        return mFurnaceKind;
    }

    ContainerType getFurnaceContainerType() const {
        return mFurnaceContainerType;
    }

    void tickFurnace(ServerNetworkHandler &owner);

    void loadFurnaceView();

    void storeFurnaceView(ServerNetworkHandler &owner);

    static void tickStoredFurnace(ServerNetworkHandler &owner, FurnaceBlockActor &furnace);

    static void onFurnaceBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position);

    bool onClientOpenChest(const Vector3i &position);

    bool onClientOpenBlockContainer(const Vector3i &position, ContainerType type);

    // a ContainerOpenPacket naming the actor; false while another window is open or it has no container
    bool openEntityContainer(ServerActor &actor, ContainerType type);

    // the actor whose container is open, -1 for a block container or none
    int64_t getContainerActorId() const {
        return mContainerActorId;
    }

    // resends the open entity container after a change made outside the window
    void refreshEntityContainer(int64_t actorId);

    bool isContainerOpen() const {
        return mContainerWindowId != CONTAINER_ID_NONE;
    }

    int getContainerWindowId() const {
        return mContainerWindowId;
    }

    const Vector3i &getContainerPosition() const {
        return mContainerPosition;
    }

    ContainerType getCurrentWindowType() const {
        return mCurrentWindowType;
    }

    // packed columns of the containers opened since the last save: written with the player when it leaves,
    // its items and theirs together
    const std::vector<std::pair<const Level *, int64_t>> &getTouchedColumns() const {
        return mTouchedColumns;
    }

    void clearTouchedColumns() {
        mTouchedColumns.clear();
    }

    bool refreshEnchantInput(const ItemStack &current, int32_t newSeed);

    int32_t getEnchantSeed() const {
        return mEnchantSeed;
    }

    Container *getContainer();

    void refreshOpenContainer(const Vector3i &position);

    void syncCraftingTableState(const std::vector<ItemStack> &recipeOutputs,
                                const std::vector<uint32_t> &recipeSourceIndices);

    void syncCraftingPreview(const std::vector<ItemStack> &recipeOutputs,
                             const std::vector<uint32_t> &recipeSourceIndices);

    void onCurrentWindowRemove();

    void onClientRemoveWindow(int windowId);

    // the player left: every open window closes now, no ContainerClose will come
    void closeAllWindows();

    void dispatchPluginClose();

    int getCurrentWindowId() const {
        return mFurnaceWindowId != CONTAINER_ID_NONE
                       ? mFurnaceWindowId
                       : (mCraftingTableWindowId != CONTAINER_ID_NONE
                                  ? mCraftingTableWindowId
                                  : (mAnvilWindowId != CONTAINER_ID_NONE
                                             ? mAnvilWindowId
                                             : (mContainerWindowId != CONTAINER_ID_NONE
                                                        ? mContainerWindowId
                                                        : (mMainInventoryWindowId != CONTAINER_ID_NONE
                                                                   ? mMainInventoryWindowId
                                                                   : mLastInventoryNetworkId))));
    }

private:
    int _getNewWindowId();

    bool _allowPluginOpen(const Vector3i &position);

    void _touchColumn(const Vector3i &position);

    int _getWindowId(InventoryId inventory) const;

    int _getSize(InventoryId inventory) const;

    void _animateChest(ChestBlockActor &chest, bool open);

    void _animateBlockContainer(const Vector3i &position, bool open, const char *openSound, const char *closeSound);

    void _animateBarrel(const Vector3i &position, bool open);

    void _closeBlockContainer();

    void _closeAnvil();

    bool _openWorkbench(ContainerType type, const Vector3i &position);

    void _sendContentPackets(int containerId, const Container &container);

    void _closeContainerWindow();

    void _sendContentPackets(int containerId, InventoryId inventory);

    void _sendSlotPackets(int containerId, int netSlot, InventoryId inventory, int slot);

    void _sendOutputPacket(const ItemStack &item);

    void _storeFurnaceState(bool clearLocal);

    static void _releaseFurnaceExperience(ServerNetworkHandler &owner, FurnaceBlockActor &furnace);

    // the player's level: any container they open lives there
    BlockActorStore *_blockActors() const;

    ServerPlayer *mPlayer;
    PacketSender *mSender;
    ServerNetworkHandler *mOwner = nullptr;
    int mLastInventoryNetworkId;
    ContainerType mCurrentWindowType;
    int mMainInventoryWindowId;
    int mCraftingTableWindowId;
    Vector3i mCraftingTablePosition;
    int mAnvilWindowId = CONTAINER_ID_NONE;
    ContainerType mAnvilWindowType = ContainerType::Anvil;
    Vector3i mAnvilPosition;
    CartographyLock mClientMapLock;
    int32_t mStonecutterSelectedRecipe = 0;
    int mContainerWindowId = CONTAINER_ID_NONE;
    Vector3i mContainerPosition;
    int64_t mContainerActorId = -1;
    std::vector<std::pair<const Level *, int64_t>> mTouchedColumns;
    std::string mEnchantInputId;
    int32_t mEnchantInputCount = 0;
    int32_t mEnchantInputDamage = 0;
    int32_t mEnchantSeed = 0;
    int mFurnaceWindowId;
    Vector3i mFurnacePosition;
    FurnaceKind mFurnaceKind;
    ContainerType mFurnaceContainerType;
    int mFurnaceBurnTime;
    int mFurnaceMaxBurnTime;
    int mFurnaceCookTime;
    std::array<ItemStack, 3> mFurnaceObservedItems;
    int mClientSelectedHotbarSlot;
    bool mHasPendingCloseWindow;
    int mPendingCloseWindowId;
    bool mHasPendingOpenMainInventory;
    bool mPluginOpen = false;
    Vector3i mPluginOpenPosition;
};
