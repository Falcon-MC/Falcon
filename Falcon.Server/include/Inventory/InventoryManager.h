#pragma once

#include "Core/Math/Vector3i.h"
#include "Inventory/Container.h"
#include "Protocol/Types/ContainerType.h"
#include "Protocol/Types/ItemStack.h"

#include <string>

#include <cstdint>
#include <array>
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

    bool openContainer(ContainerType type, const Vector3i &position);

    bool isCraftingTableOpen() const { return mCraftingTableWindowId != CONTAINER_ID_NONE; }

    bool isFurnaceOpen() const { return mFurnaceWindowId != CONTAINER_ID_NONE; }

    int getCraftingTableWindowId() const { return mCraftingTableWindowId; }

    int getFurnaceWindowId() const { return mFurnaceWindowId; }

    const Vector3i &getFurnacePosition() const { return mFurnacePosition; }

    FurnaceKind getFurnaceKind() const { return mFurnaceKind; }

    ContainerType getFurnaceContainerType() const { return mFurnaceContainerType; }

    void tickFurnace(ServerNetworkHandler &owner);

    void loadFurnaceView();

    void storeFurnaceView(ServerNetworkHandler &owner);

    static void tickStoredFurnace(ServerNetworkHandler &owner, FurnaceBlockActor &furnace);

    static void onFurnaceBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position);

    bool onClientOpenChest(const Vector3i &position);

    bool onClientOpenBlockContainer(const Vector3i &position, ContainerType type);

    bool isContainerOpen() const { return mContainerWindowId != CONTAINER_ID_NONE; }

    int getContainerWindowId() const { return mContainerWindowId; }

    const Vector3i &getContainerPosition() const { return mContainerPosition; }

    ContainerType getCurrentWindowType() const { return mCurrentWindowType; }

    bool refreshEnchantInput(const ItemStack &current, int32_t newSeed);

    int32_t getEnchantSeed() const { return mEnchantSeed; }

    Container *getContainer();

    void refreshOpenContainer(const Vector3i &position);

    void syncCraftingTableState(const std::vector<ItemStack> &recipeOutputs,
                                const std::vector<uint32_t> &recipeSourceIndices);

    void syncCraftingPreview(const std::vector<ItemStack> &recipeOutputs,
                             const std::vector<uint32_t> &recipeSourceIndices);

    void onCurrentWindowRemove();

    void onClientRemoveWindow(int windowId);

    void dispatchPluginClose();

    int getCurrentWindowId() const {
        return mFurnaceWindowId != CONTAINER_ID_NONE
               ? mFurnaceWindowId
               : (mCraftingTableWindowId != CONTAINER_ID_NONE
                      ? mCraftingTableWindowId
                      : (mContainerWindowId != CONTAINER_ID_NONE
                             ? mContainerWindowId
                             : (mMainInventoryWindowId != CONTAINER_ID_NONE
                                    ? mMainInventoryWindowId
                                    : mLastInventoryNetworkId)));
    }

private:
    int _getNewWindowId();

    bool _allowPluginOpen(const Vector3i &position);

    int _getWindowId(InventoryId inventory) const;

    int _getSize(InventoryId inventory) const;

    void _animateChest(ChestBlockActor &chest, bool open);

    void _animateBlockContainer(const Vector3i &position, bool open, const char *openSound,
                                const char *closeSound);

    void _animateBarrel(const Vector3i &position, bool open);

    void _closeBlockContainer();

    void _sendContentPackets(int containerId, const Container &container);

    void _sendContentPackets(int containerId, InventoryId inventory);

    void _sendSlotPackets(int containerId, int netSlot, InventoryId inventory, int slot);

    void _sendOutputPacket(const ItemStack &item);

    void _storeFurnaceState(bool clearLocal);

    static void _releaseFurnaceExperience(ServerNetworkHandler &owner, FurnaceBlockActor &furnace);

    /** Block actors of the level the player is in, where any container they open lives. */
    BlockActorStore *_blockActors() const;

    ServerPlayer *mPlayer;
    PacketSender *mSender;
    ServerNetworkHandler *mOwner = nullptr;
    int mLastInventoryNetworkId;
    ContainerType mCurrentWindowType;
    int mMainInventoryWindowId;
    int mCraftingTableWindowId;
    Vector3i mCraftingTablePosition;
    int mContainerWindowId = CONTAINER_ID_NONE;
    Vector3i mContainerPosition;
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
