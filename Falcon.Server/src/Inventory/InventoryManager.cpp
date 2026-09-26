#include "Inventory/InventoryManager.h"
#include "Actor/Misc/ItemActor.h"
#include "Block/Actor/ChestBlockActor.h"
#include "Block/Actor/ContainerBlockActor.h"
#include "Block/Actor/EnderChestBlockActor.h"
#include "Block/Actor/FurnaceBlockActor.h"
#include "Block/BlockActorStore.h"
#include "Block/Systems/RedstoneSystem.h"
#include "Actor/ServerPlayer.h"
#include "Block/Block.h"
#include "Block/Blocks/FenceBlock.h"
#include "Block/Blocks/FenceGateBlock.h"
#include "Block/Blocks/FurnaceBlock.h"
#include "Block/Blocks/SlabBlock.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Inventory/FurnaceInventory.h"
#include "Inventory/BundleInventory.h"
#include "Item/CraftingRecipeTable.h"
#include "Item/FurnaceExperience.h"
#include "Inventory/CraftingManager.h"
#include "Level/Level.h"
#include "Block/Inventory/CraftingTableInventory.h"
#include "Network/PacketSender.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Plugin/PluginManager.h"
#include "Protocol/BlockStateHasher.h"
#include "Protocol/Packets/BlockActorDataPacket.h"
#include "Protocol/Packets/ContainerClosePacket.h"
#include "Protocol/Packets/BlockEventPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Packets/ContainerOpenPacket.h"
#include "Protocol/Packets/ContainerSetDataPacket.h"
#include "Protocol/Packets/UpdateBlockPacket.h"
#include "Protocol/Packets/InventoryContentPacket.h"
#include "Protocol/Packets/InventorySlotPacket.h"
#include "Protocol/Packets/MobEquipmentPacket.h"
#include "Protocol/Types/ContainerSlotType.h"

#include <algorithm>
#include <array>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
    const int32_t CHEST_ANIMATION_EVENT_TYPE = 1;

    const ItemStack &itemAt(const PlayerInventory &inventory, InventoryManager::InventoryId id, int slot) {
        switch (id) {
            case InventoryManager::InventoryId::Inventory:
                return inventory.getItem(slot);
            case InventoryManager::InventoryId::Offhand:
                return inventory.getOffhand();
            case InventoryManager::InventoryId::Armor:
                return inventory.getArmor(slot);
            case InventoryManager::InventoryId::Cursor:
                return inventory.getCursor();
            case InventoryManager::InventoryId::CraftingInput:
                return inventory.getCraftingItem(slot);
            case InventoryManager::InventoryId::CraftingTableInput:
                return inventory.getCraftingTableItem(slot);
            case InventoryManager::InventoryId::FurnaceInput:
                return inventory.getFurnaceItem(slot);
        }
        return PlayerInventory::getEmptyItem();
    }

    class PlayerInventoryView final : public Container {
    public:
        PlayerInventoryView(const PlayerInventory &inventory, InventoryManager::InventoryId id, int size)
                : mInventory(inventory), mId(id), mSize(size) {}

        int getContainerSize() const override { return mSize; }

        const ItemStack &getContainerItem(int slot) const override { return itemAt(mInventory, mId, slot); }

        void setContainerItem(int slot, ItemStack item) override {
            (void) slot;
            (void) item;
        }

    private:
        const PlayerInventory &mInventory;
        InventoryManager::InventoryId mId;
        int mSize;
    };

    bool furnaceItemsEqual(const ItemStack &left, const ItemStack &right) {
        return left.mDefinition == right.mDefinition && left.mBlockDefinition == right.mBlockDefinition
               && left.mCount == right.mCount && left.mDamage == right.mDamage && left.mTag == right.mTag;
    }
}

InventoryManager::InventoryManager()
        : mPlayer(nullptr), mSender(nullptr), mLastInventoryNetworkId(CONTAINER_ID_FIRST),
          mCurrentWindowType(ContainerType::Container), mMainInventoryWindowId(CONTAINER_ID_NONE),
          mCraftingTableWindowId(CONTAINER_ID_NONE), mCraftingTablePosition(0, 0, 0),
          mFurnaceWindowId(CONTAINER_ID_NONE), mFurnacePosition(0, 0, 0),
          mFurnaceKind(FurnaceKind::Furnace), mFurnaceContainerType(ContainerType::Furnace),
          mFurnaceBurnTime(0), mFurnaceMaxBurnTime(0), mFurnaceCookTime(0),
          mClientSelectedHotbarSlot(-1), mHasPendingCloseWindow(false), mPendingCloseWindowId(CONTAINER_ID_NONE),
          mHasPendingOpenMainInventory(false) {}

void InventoryManager::attach(ServerPlayer *player, ServerNetworkHandler *owner) {
    mPlayer = player;
    mOwner = owner;
    mSender = owner;
    mLastInventoryNetworkId = CONTAINER_ID_FIRST;
    mCurrentWindowType = ContainerType::Container;
    mMainInventoryWindowId = CONTAINER_ID_NONE;
    mCraftingTableWindowId = CONTAINER_ID_NONE;
    mCraftingTablePosition = Vector3i(0, 0, 0);
    mFurnaceWindowId = CONTAINER_ID_NONE;
    mFurnacePosition = Vector3i(0, 0, 0);
    mFurnaceKind = FurnaceKind::Furnace;
    mFurnaceContainerType = ContainerType::Furnace;
    mFurnaceBurnTime = 0;
    mFurnaceMaxBurnTime = 0;
    mFurnaceCookTime = 0;
    for (ItemStack &item: mFurnaceObservedItems)
        item = ItemStack::air();
    mClientSelectedHotbarSlot = -1;
    mHasPendingCloseWindow = false;
    mPendingCloseWindowId = CONTAINER_ID_NONE;
    mHasPendingOpenMainInventory = false;
}

int InventoryManager::_getNewWindowId() {
    int next = (mLastInventoryNetworkId + 1) % CONTAINER_ID_LAST;
    if (next < CONTAINER_ID_FIRST) {
        next = CONTAINER_ID_FIRST;
    }

    mLastInventoryNetworkId = next;
    return mLastInventoryNetworkId;
}

int InventoryManager::_getWindowId(InventoryId inventory) const {
    switch (inventory) {
        case InventoryId::Inventory:
            return CONTAINER_ID_INVENTORY;
        case InventoryId::Offhand:
            return CONTAINER_ID_OFFHAND;
        case InventoryId::Armor:
            return CONTAINER_ID_ARMOR;
        case InventoryId::Cursor:
            return CONTAINER_ID_UI;
        case InventoryId::CraftingInput:
            return CONTAINER_ID_UI;
        case InventoryId::CraftingTableInput:
            return CONTAINER_ID_UI;
        case InventoryId::FurnaceInput:
            return mFurnaceWindowId;
    }
    return CONTAINER_ID_NONE;
}

int InventoryManager::_getSize(InventoryId inventory) const {
    switch (inventory) {
        case InventoryId::Inventory:
            return PlayerInventory::CONTAINER_SIZE;
        case InventoryId::Armor:
            return PlayerInventory::ARMOR_SIZE;
        case InventoryId::Offhand:
        case InventoryId::Cursor:
            return 1;
        case InventoryId::CraftingInput:
            return PlayerInventory::CRAFTING_SIZE;
        case InventoryId::CraftingTableInput:
            return CraftingTableInventory::SIZE;
        case InventoryId::FurnaceInput:
            return FurnaceInventory::SIZE;
    }
    return 0;
}

void InventoryManager::_sendContentPackets(int containerId, const Container &container) {
    const int size = container.getContainerSize();

    InventoryContentPacket content;
    content.mContainerId = containerId;
    content.mContents.reserve((size_t) size);
    for (int slot = 0; slot < size; slot++) {
        content.mContents.push_back(container.getContainerItem(slot));
    }
    content.mContainerNameData.mContainer = ContainerSlotType::AnvilInput;
    content.mStorageItem = ItemStack::air();
    mSender->sendPacketTo(mPlayer->getNetworkIdentifier(), content);
}

void InventoryManager::_sendContentPackets(int containerId, InventoryId inventory) {
    _sendContentPackets(containerId, PlayerInventoryView(mPlayer->getInventory(), inventory,
                                                         _getSize(inventory)));
}

void InventoryManager::_sendSlotPackets(int containerId, int netSlot, InventoryId inventory, int slot) {
    InventorySlotPacket packet;
    packet.mContainerId = containerId;
    packet.mSlot = netSlot;
    packet.mItem = itemAt(mPlayer->getInventory(), inventory, slot);
    mSender->sendPacketTo(mPlayer->getNetworkIdentifier(), packet);
}

void InventoryManager::syncContents(InventoryId inventory) {
    if (mPlayer == nullptr || mSender == nullptr) {
        return;
    }

    if (inventory == InventoryId::Cursor) {
        _sendSlotPackets(CONTAINER_ID_UI, UI_SLOT_CURSOR, inventory, 0);
        return;
    }

    if (inventory == InventoryId::CraftingInput) {
        for (int slot = 0; slot < PlayerInventory::CRAFTING_SIZE; slot++) {
            _sendSlotPackets(CONTAINER_ID_UI, PlayerInventory::CRAFTING_NETWORK_SLOT_FIRST + slot, inventory, slot);
        }
        return;
    }

    if (inventory == InventoryId::CraftingTableInput) {
        if (!isCraftingTableOpen()) {
            return;
        }
        for (int slot = 0; slot < CraftingTableInventory::SIZE; slot++) {
            _sendSlotPackets(CONTAINER_ID_UI, CraftingTableInventory::NETWORK_SLOT_FIRST + slot, inventory, slot);
        }
        return;
    }

    if (inventory == InventoryId::FurnaceInput) {
        if (!isFurnaceOpen()) {
            return;
        }
        _sendContentPackets(mFurnaceWindowId, inventory);
        return;
    }

    _sendContentPackets(_getWindowId(inventory), inventory);
}

void InventoryManager::syncSlot(InventoryId inventory, int slot) {
    if (mPlayer == nullptr || mSender == nullptr) {
        return;
    }

    if (inventory == InventoryId::Cursor) {
        _sendSlotPackets(CONTAINER_ID_UI, UI_SLOT_CURSOR, inventory, 0);
        return;
    }

    if (inventory == InventoryId::CraftingInput) {
        if (slot < 0 || slot >= PlayerInventory::CRAFTING_SIZE) {
            return;
        }
        _sendSlotPackets(CONTAINER_ID_UI, PlayerInventory::CRAFTING_NETWORK_SLOT_FIRST + slot, inventory, slot);
        return;
    }

    if (inventory == InventoryId::CraftingTableInput) {
        if (!isCraftingTableOpen() || slot < 0 || slot >= CraftingTableInventory::SIZE) {
            return;
        }
        _sendSlotPackets(CONTAINER_ID_UI, CraftingTableInventory::NETWORK_SLOT_FIRST + slot, inventory, slot);
        return;
    }

    if (inventory == InventoryId::FurnaceInput) {
        if (!isFurnaceOpen() || slot < 0 || slot >= FurnaceInventory::SIZE) {
            return;
        }
        _sendSlotPackets(mFurnaceWindowId, slot, inventory, slot);
        return;
    }

    if (inventory == InventoryId::Offhand) {
        _sendContentPackets(CONTAINER_ID_OFFHAND, inventory);
        return;
    }

    _sendSlotPackets(_getWindowId(inventory), slot, inventory, slot);
}

void InventoryManager::syncBundle(const ItemStack &bundle, int32_t bundleId,
                                  const std::vector<ItemStack> &contents) {
    if (mPlayer == nullptr || mSender == nullptr || bundleId < 0) {
        return;
    }

    InventoryContentPacket content;
    content.mContainerId = CONTAINER_ID_REGISTRY;
    content.mContents = contents;
    content.mContainerNameData.mContainer = ContainerSlotType::DynamicContainer;
    content.mContainerNameData.mHasDynamicId = true;
    content.mContainerNameData.mDynamicId = bundleId;
    content.mStorageItem = bundle;
    mSender->sendPacketTo(mPlayer->getNetworkIdentifier(), content);
}

void InventoryManager::syncBundles() {
    if (mPlayer == nullptr || mSender == nullptr || mOwner == nullptr) {
        return;
    }

    const PlayerInventory &inventory = mPlayer->getInventory();
    for (int slot = 0; slot < PlayerInventory::CONTAINER_SIZE; ++slot) {
        const ItemStack &item = inventory.getItem(slot);
        if (!BundleInventory::isBundle(item)) {
            continue;
        }
        syncBundle(item, BundleInventory::getBundleId(item),
                   BundleInventory::readContents(item, mOwner->getCodecContext()));
    }

    const ItemStack &offhand = inventory.getOffhand();
    if (BundleInventory::isBundle(offhand)) {
        syncBundle(offhand, BundleInventory::getBundleId(offhand),
                   BundleInventory::readContents(offhand, mOwner->getCodecContext()));
    }

    const ItemStack &cursor = inventory.getCursor();
    if (BundleInventory::isBundle(cursor)) {
        syncBundle(cursor, BundleInventory::getBundleId(cursor),
                   BundleInventory::readContents(cursor, mOwner->getCodecContext()));
    }

    Container *container = getContainer();
    if (container == nullptr) {
        return;
    }

    const int containerSize = container->getContainerSize();
    for (int slot = 0; slot < containerSize; ++slot) {
        const ItemStack &item = container->getContainerItem(slot);
        if (!BundleInventory::isBundle(item)) {
            continue;
        }
        syncBundle(item, BundleInventory::getBundleId(item),
                   BundleInventory::readContents(item, mOwner->getCodecContext()));
    }
}

void InventoryManager::syncAll() {
    if (mPlayer == nullptr || mSender == nullptr) {
        return;
    }

    syncContents(InventoryId::Inventory);
    syncContents(InventoryId::Offhand);
    syncContents(InventoryId::Armor);
    syncContents(InventoryId::CraftingInput);
    syncContents(InventoryId::Cursor);
    syncBundles();
    if (isFurnaceOpen()) {
        syncContents(InventoryId::FurnaceInput);
    }
}

BlockActorStore *InventoryManager::_blockActors() const {
    if (mOwner == nullptr || mPlayer == nullptr)
        return nullptr;

    return &mOwner->getLevelFor(*mPlayer).getBlockActors();
}

void InventoryManager::_storeFurnaceState(bool clearLocal) {
    BlockActorStore *blockActors = _blockActors();
    if (mPlayer == nullptr || !isFurnaceOpen() || blockActors == nullptr) {
        return;
    }

    FurnaceBlockActor &state = blockActors->getOrCreate<FurnaceBlockActor>(mFurnacePosition);
    state.mKind = mFurnaceKind;
    for (int slot = 0; slot < FurnaceInventory::SIZE; ++slot) {
        const ItemStack &local = mPlayer->getInventory().getFurnaceItem(slot);
        const ItemStack &observed = mFurnaceObservedItems[(size_t) slot];
        if (!furnaceItemsEqual(local, observed)) {
            state.mInventory.mItems[(size_t) slot] = local;
            if (slot == FurnaceInventory::SLOT_OUTPUT && !observed.isAir()
                && (local.isAir() || local.mCount < observed.mCount))
                _releaseFurnaceExperience(*mOwner, state);
        }
        mFurnaceObservedItems[(size_t) slot] = local;
        if (clearLocal) {
            mPlayer->getInventory().setFurnaceItem(slot, ItemStack::air());
            mFurnaceObservedItems[(size_t) slot] = ItemStack::air();
        }
    }
}

void InventoryManager::_releaseFurnaceExperience(ServerNetworkHandler &owner, FurnaceBlockActor &furnace) {
    Level *level = furnace.getLevel();
    const int32_t experience = furnace.takeStoredExperience();
    if (level == nullptr || experience <= 0)
        return;

    const Vector3i &position = furnace.getPosition();
    owner.spawnExperienceOrbs(*level, Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f,
                                               (float) position.z + 0.5f), experience);
}

namespace {
    bool isWoodLike(const std::string &id) {
        return id == "minecraft:wood" || id == "minecraft:crimson_stem" || id == "minecraft:warped_stem"
               || id == "minecraft:crimson_hyphae" || id == "minecraft:warped_hyphae"
               || id.find("oak_") != std::string::npos || id.find("spruce_") != std::string::npos
               || id.find("birch_") != std::string::npos || id.find("jungle_") != std::string::npos
               || id.find("acacia_") != std::string::npos || id.find("dark_oak_") != std::string::npos
               || id.find("mangrove_") != std::string::npos || id.find("cherry_") != std::string::npos
               || id.find("bamboo_") != std::string::npos || id.find("crimson_") != std::string::npos
               || id.find("warped_") != std::string::npos || id.find("pale_oak_") != std::string::npos
               || id.find("poplar_") != std::string::npos;
    }

    int fuelTime(const ItemStack &item) {
        if (item.isAir() || item.mDefinition == nullptr) {
            return 0;
        }
        const std::string id = item.mDefinition->getIdentifier();
        if (id == "minecraft:lava_bucket") return 20000;
        if (id == "minecraft:coal_block") return 16000;
        if (id == "minecraft:coal" || id == "minecraft:charcoal") return 1600;
        if (id == "minecraft:blaze_rod") return 2400;
        if (id == "minecraft:dried_kelp_block") return 4000;
        if (id == "minecraft:bamboo") return 50;
        if (id == "minecraft:stick") return 100;
        if (id == "minecraft:bowl" || id == "minecraft:bow") return 200;
        if (id.find("_boat") != std::string::npos) return 1200;
        if (id == "minecraft:wooden_sword" || id == "minecraft:wooden_pickaxe"
            || id == "minecraft:wooden_axe" || id == "minecraft:wooden_shovel"
            || id == "minecraft:wooden_hoe") return 200;
        if (isWoodLike(id) && VanillaBlocks::getAs<SlabBlock>(id) != nullptr) return 300;
        if (isWoodLike(id) && (id.find("_planks") != std::string::npos || id.find("_log") != std::string::npos
            || id.find("_wood") != std::string::npos || id.find("_stem") != std::string::npos
            || id.find("_hyphae") != std::string::npos || id.find("_mosaic") != std::string::npos
            || id == "minecraft:wood")) return 300;
        if (isWoodLike(id) && id.find("_sign") != std::string::npos) return 200;
        if (isWoodLike(id) && id.find("_door") != std::string::npos) return 200;
        if (isWoodLike(id) && id.find("_button") != std::string::npos) return 100;
        if (isWoodLike(id) && (id.find("_trapdoor") != std::string::npos
            || VanillaBlocks::getAs<FenceBlock>(id) != nullptr
            || VanillaBlocks::getAs<FenceGateBlock>(id) != nullptr
            || id.find("_pressure_plate") != std::string::npos
            || id.find("_stairs") != std::string::npos)) return 300;
        if (id.find("wooden_") != std::string::npos) return 200;
        if (id.find("_sapling") != std::string::npos) return 100;
        if (id == "minecraft:bookshelf" || id == "minecraft:crafting_table" || id == "minecraft:cartography_table"
            || id == "minecraft:fletching_table" || id == "minecraft:smithing_table" || id == "minecraft:chest"
            || id == "minecraft:trapped_chest" || id == "minecraft:barrel" || id == "minecraft:jukebox"
            || id == "minecraft:note_block" || id == "minecraft:daylight_detector"
            || id == "minecraft:banner" || id.find("_banner") != std::string::npos
            || id.find("_hanging_sign") != std::string::npos) return 300;
        return 0;
    }

    bool stackMatches(const ItemStack &left, const ItemStack &right) {
        return !left.isAir() && !right.isAir() && PlayerInventory::canStack(left, right);
    }

    std::string litIdentifier(const std::string &identifier, bool lit) {
        if (identifier == "minecraft:furnace" || identifier == "minecraft:lit_furnace") {
            return lit ? "minecraft:lit_furnace" : "minecraft:furnace";
        }
        if (identifier == "minecraft:blast_furnace" || identifier == "minecraft:lit_blast_furnace") {
            return lit ? "minecraft:lit_blast_furnace" : "minecraft:blast_furnace";
        }
        if (identifier == "minecraft:smoker" || identifier == "minecraft:lit_smoker") {
            return lit ? "minecraft:lit_smoker" : "minecraft:smoker";
        }
        return identifier;
    }

    void updateLitState(ServerNetworkHandler &owner, Level &level, const Vector3i &position, bool lit) {
        const BlockState state = level.getBlockState(position.x, position.y, position.z);
        const std::string identifier = litIdentifier(state.mName, lit);
        if (identifier == state.mName) {
            return;
        }
        const BlockState updated(identifier, state.mStates);
        level.setBlockState(position.x, position.y, position.z, updated);
        UpdateBlockPacket packet;
        packet.mBlockPosition = position;
        packet.mRuntimeId = (uint32_t) BlockStateHasher::hash(updated.mName, updated.mStates);
        packet.mFlags = UpdateBlockPacket::Flag::All;
        packet.mDataLayer = 0;
        BlockActionHandler::broadcastToViewers(owner, level,
                                               Vector3f((float) position.x + 0.5f,
                                                        (float) position.y + 0.5f,
                                                        (float) position.z + 0.5f),
                                               packet);
    }

    const FurnaceRecipeData *findFurnaceRecipe(const ItemStack &input) {
        if (input.isAir() || input.mDefinition == nullptr) {
            return nullptr;
        }

        const FurnaceRecipeData *recipes = CraftingRecipeTable::getFurnaceRecipes();
        const size_t recipeCount = CraftingRecipeTable::getFurnaceRecipeCount();
        for (size_t index = 0; index < recipeCount; ++index) {
            const FurnaceRecipeData &candidate = recipes[index];
            if (candidate.mInputItemId == nullptr
                || std::string(input.mDefinition->getIdentifier()) != candidate.mInputItemId
                || (candidate.mInputAuxValue >= 0 && input.mDamage != candidate.mInputAuxValue)
                || input.mCount < candidate.mInputCount) {
                continue;
            }
            return &candidate;
        }
        return nullptr;
    }

    bool sameFurnaceItem(const ItemStack &left, const ItemStack &right) {
        return furnaceItemsEqual(left, right);
    }

    bool tickFurnaceState(ServerNetworkHandler &owner, const Vector3i &position, FurnaceBlockActor &state) {
        Level *level = state.getLevel();
        if (level == nullptr || !FurnaceBlock::matches(level->getBlockState(position.x, position.y, position.z))) {
            state.mBurnTime = 0;
            state.mCookTime = 0;
            state.mMaxBurnTime = 0;
            return false;
        }

        const FurnaceRecipeData *recipe = findFurnaceRecipe(state.mInventory.mItems[FurnaceInventory::SLOT_INPUT]);
        ItemStack result = ItemStack::air();
        if (recipe != nullptr) {
            result.mDefinition = owner.getItemDefinitions().getDefinition(recipe->mOutputItemId);
            result.mBlockDefinition = owner.getBlockDefinitions().getDefinition(recipe->mOutputItemId);
            result.mDamage = 0;
            result.mCount = recipe->mOutputCount;
        }

        const ItemStack &input = state.mInventory.mItems[FurnaceInventory::SLOT_INPUT];
        const ItemStack &currentOutput = state.mInventory.mItems[FurnaceInventory::SLOT_OUTPUT];
        const bool canSmelt = recipe != nullptr && !result.isAir() && (currentOutput.isAir()
                             || (stackMatches(currentOutput, result)
                                 && currentOutput.mCount + result.mCount
                                    <= PlayerInventory::getMaxStackSize(currentOutput)));
        const int previousBurn = state.mBurnTime;
        const int previousCook = state.mCookTime;
        const int previousMaxBurn = state.mMaxBurnTime;
        const std::array<ItemStack, FurnaceInventory::SIZE> previousItems = state.mInventory.mItems;

        if (state.mBurnTime <= 0 && canSmelt) {
            ItemStack fuel = state.mInventory.mItems[FurnaceInventory::SLOT_FUEL];
            int duration = fuelTime(fuel);
            if (duration > 0 && fuel.mCount > 0) {
                if (PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_FURNACE_BURN)) {
                    ItemStack burning = fuel;
                    PluginEvent burnEvent;
                    burnEvent.mType = FALCON_EVENT_FURNACE_BURN;
                    burnEvent.mCancellable = true;
                    burnEvent.mLevel = level;
                    burnEvent.mBlockPosition = position;
                    burnEvent.mItem = &burning;
                    burnEvent.mAmount = (double) duration;
                    plugins->dispatch(burnEvent);
                    duration = burnEvent.mCancelled ? 0 : std::max(0, (int) burnEvent.mAmount);
                }
            }
            if (duration > 0 && fuel.mCount > 0) {
                state.mBurnTime = duration;
                state.mMaxBurnTime = duration;
                --fuel.mCount;
                if (fuel.mCount <= 0) {
                    if (fuel.mDefinition != nullptr
                        && fuel.mDefinition->getIdentifier() == "minecraft:lava_bucket") {
                        fuel = ItemStack::air();
                        fuel.mDefinition = owner.getItemDefinitions().getDefinition("minecraft:bucket");
                        fuel.mBlockDefinition = owner.getBlockDefinitions().getDefinition("minecraft:bucket");
                        fuel.mCount = fuel.mDefinition == nullptr ? 0 : 1;
                    } else {
                        fuel = ItemStack::air();
                    }
                }
                state.mInventory.mItems[FurnaceInventory::SLOT_FUEL] = std::move(fuel);
            }
        }

        if (state.mBurnTime > 0) {
            --state.mBurnTime;
            if (canSmelt) {
                ++state.mCookTime;
                const int duration = FurnaceInventory::cookDuration(state.mKind);
                if (state.mCookTime >= duration) {
                    ItemStack smelted = result;
                    bool allowed = true;
                    if (PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_FURNACE_SMELT)) {
                        ItemStack source = input;
                        PluginEvent smeltEvent;
                        smeltEvent.mType = FALCON_EVENT_FURNACE_SMELT;
                        smeltEvent.mCancellable = true;
                        smeltEvent.mLevel = level;
                        smeltEvent.mBlockPosition = position;
                        smeltEvent.mItem = &source;
                        smeltEvent.mResult = &smelted;
                        plugins->dispatch(smeltEvent);
                        allowed = !smeltEvent.mCancelled && !smelted.isAir()
                                  && (currentOutput.isAir() || stackMatches(currentOutput, smelted));
                    }

                    if (allowed) {
                        if (input.mDefinition != nullptr)
                            state.mStoredExperience += FurnaceExperience::get(input.mDefinition->getIdentifier());

                        ItemStack produced = smelted;
                        if (!currentOutput.isAir())
                            produced.mCount += currentOutput.mCount;
                        state.mInventory.mItems[FurnaceInventory::SLOT_OUTPUT] = std::move(produced);

                        ItemStack consumed = input;
                        consumed.mCount -= recipe->mInputCount;
                        if (consumed.mCount <= 0)
                            consumed = ItemStack::air();
                        state.mInventory.mItems[FurnaceInventory::SLOT_INPUT] = std::move(consumed);
                    }
                    state.mCookTime -= duration;
                }
            } else if (state.mBurnTime <= 0) {
                state.mBurnTime = 0;
                state.mCookTime = 0;
                state.mMaxBurnTime = 0;
            } else {
                state.mCookTime = 0;
            }
        } else {
            state.mBurnTime = 0;
            state.mCookTime = 0;
            state.mMaxBurnTime = 0;
        }

        const std::string currentIdentifier = level->getBlockState(position.x, position.y, position.z).mName;
        const bool isLit = currentIdentifier == "minecraft:lit_furnace"
                           || currentIdentifier == "minecraft:lit_blast_furnace"
                           || currentIdentifier == "minecraft:lit_smoker";
        if (state.mBurnTime > 0 && (previousBurn == 0 || !isLit))
            updateLitState(owner, *level, position, true);
        else if (state.mBurnTime == 0 && isLit)
            updateLitState(owner, *level, position, false);

        if (previousBurn == state.mBurnTime && previousCook == state.mCookTime
            && previousMaxBurn == state.mMaxBurnTime) {
            for (int slot = 0; slot < FurnaceInventory::SIZE; ++slot) {
                if (!sameFurnaceItem(previousItems[(size_t) slot], state.mInventory.mItems[(size_t) slot]))
                    return true;
            }
            return false;
        }
        return true;
    }
}

void InventoryManager::tickFurnace(ServerNetworkHandler &owner) {
    if (mPlayer == nullptr || !isFurnaceOpen()) {
        return;
    }

    Level &level = owner.getLevelFor(*mPlayer);
    if (!FurnaceBlock::matches(level.getBlockState(mFurnacePosition.x, mFurnacePosition.y, mFurnacePosition.z))) {
        onClientRemoveWindow(mFurnaceWindowId);
        return;
    }

    FurnaceBlockActor &state = level.getBlockActors().getOrCreate<FurnaceBlockActor>(mFurnacePosition);
    PlayerInventory &inventory = mPlayer->getInventory();
    const int previousBurn = mFurnaceBurnTime;
    const int previousMaxBurn = mFurnaceMaxBurnTime;
    const int previousCook = mFurnaceCookTime;
    const std::array<ItemStack, FurnaceInventory::SIZE> previousItems = {
        inventory.getFurnaceItem(FurnaceInventory::SLOT_INPUT),
        inventory.getFurnaceItem(FurnaceInventory::SLOT_FUEL),
        inventory.getFurnaceItem(FurnaceInventory::SLOT_OUTPUT)
    };
    mFurnaceBurnTime = state.mBurnTime;
    mFurnaceMaxBurnTime = state.mMaxBurnTime;
    mFurnaceCookTime = state.mCookTime;
    for (int slot = 0; slot < FurnaceInventory::SIZE; ++slot) {
        if (!sameFurnaceItem(previousItems[(size_t) slot], state.mInventory.mItems[(size_t) slot]))
            inventory.setFurnaceItem(slot, state.mInventory.mItems[(size_t) slot]);
        mFurnaceObservedItems[(size_t) slot] = inventory.getFurnaceItem(slot);
    }
    if (!sameFurnaceItem(previousItems[FurnaceInventory::SLOT_INPUT],
                         inventory.getFurnaceItem(FurnaceInventory::SLOT_INPUT))) {
        syncSlot(InventoryId::FurnaceInput, FurnaceInventory::SLOT_INPUT);
    }
    if (!sameFurnaceItem(previousItems[FurnaceInventory::SLOT_FUEL],
                         inventory.getFurnaceItem(FurnaceInventory::SLOT_FUEL))) {
        syncSlot(InventoryId::FurnaceInput, FurnaceInventory::SLOT_FUEL);
    }
    if (!sameFurnaceItem(previousItems[FurnaceInventory::SLOT_OUTPUT],
                         inventory.getFurnaceItem(FurnaceInventory::SLOT_OUTPUT))) {
        syncSlot(InventoryId::FurnaceInput, FurnaceInventory::SLOT_OUTPUT);
    }
    if (previousCook != mFurnaceCookTime) {
        ContainerSetDataPacket packet;
        packet.mWindowId = (int8_t) mFurnaceWindowId;
        packet.mProperty = 0;
        packet.mValue = mFurnaceCookTime;
        mSender->sendPacketTo(mPlayer->getNetworkIdentifier(), packet);
    }
    if (previousBurn != mFurnaceBurnTime) {
        ContainerSetDataPacket packet;
        packet.mWindowId = (int8_t) mFurnaceWindowId;
        packet.mProperty = 1;
        packet.mValue = mFurnaceBurnTime;
        mSender->sendPacketTo(mPlayer->getNetworkIdentifier(), packet);
    }
    if (previousMaxBurn != mFurnaceMaxBurnTime) {
        ContainerSetDataPacket packet;
        packet.mWindowId = (int8_t) mFurnaceWindowId;
        packet.mProperty = 2;
        packet.mValue = mFurnaceMaxBurnTime;
        mSender->sendPacketTo(mPlayer->getNetworkIdentifier(), packet);
    }
}

void InventoryManager::loadFurnaceView() {
    BlockActorStore *blockActors = _blockActors();
    if (mPlayer == nullptr || !isFurnaceOpen() || blockActors == nullptr)
        return;

    const FurnaceBlockActor &state = blockActors->getOrCreate<FurnaceBlockActor>(mFurnacePosition);
    for (int slot = 0; slot < FurnaceInventory::SIZE; ++slot) {
        mPlayer->getInventory().setFurnaceItem(slot, state.mInventory.mItems[(size_t) slot]);
        mFurnaceObservedItems[(size_t) slot] = state.mInventory.mItems[(size_t) slot];
    }
}

void InventoryManager::storeFurnaceView(ServerNetworkHandler &owner) {
    if (mPlayer == nullptr || !isFurnaceOpen())
        return;

    _storeFurnaceState(false);

    Level &level = owner.getLevelFor(*mPlayer);
    for (auto &entry: owner.getPlayers()) {
        InventoryManager &manager = entry.second.getInventoryManager();
        if (&manager != this && manager.isFurnaceOpen() && manager.getFurnacePosition() == mFurnacePosition
            && &owner.getLevelFor(entry.second) == &level)
            manager.tickFurnace(owner);
    }
}

void InventoryManager::tickStoredFurnace(ServerNetworkHandler &owner, FurnaceBlockActor &furnace) {
    tickFurnaceState(owner, furnace.getPosition(), furnace);
}

void InventoryManager::onFurnaceBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
    for (auto &entry: owner.getPlayers()) {
        InventoryManager &manager = entry.second.getInventoryManager();
        if (manager.isFurnaceOpen() && manager.getFurnacePosition() == position
            && &owner.getLevelFor(entry.second) == &level)
            manager.onClientRemoveWindow(manager.getFurnaceWindowId());
    }

    FurnaceBlockActor *furnace = level.getBlockActors().find<FurnaceBlockActor>(position);
    if (furnace == nullptr)
        return;

    const Vector3f dropPosition((float) position.x + 0.5f, (float) position.y + 0.5f,
                                (float) position.z + 0.5f);
    for (const ItemStack &item: furnace->mInventory.mItems) {
        if (!item.isAir() && item.mCount > 0)
            owner.dropItem(level, dropPosition, item, Vector3f(0.0f, 0.2f, 0.0f), ItemActor::DEFAULT_PICKUP_DELAY);
    }
    _releaseFurnaceExperience(owner, *furnace);
    level.getBlockActors().remove(position);
}

void InventoryManager::_sendOutputPacket(const ItemStack &item) {
    InventorySlotPacket packet;
    packet.mContainerId = CONTAINER_ID_UI;
    packet.mSlot = CraftingTableInventory::CREATED_OUTPUT_NETWORK_SLOT;
    packet.mItem = item;
    mSender->sendPacketTo(mPlayer->getNetworkIdentifier(), packet);
}

void InventoryManager::syncCraftingTableState(const std::vector<ItemStack> &recipeOutputs,
                                               const std::vector<uint32_t> &recipeSourceIndices) {
    if (mPlayer == nullptr || mSender == nullptr || !isCraftingTableOpen()) {
        return;
    }

    syncContents(InventoryId::CraftingTableInput);
    const ItemStack result = CraftingManager::findResult(mPlayer->getInventory().getCraftingTableContents(), 3,
                                                         recipeOutputs, recipeSourceIndices);
    _sendOutputPacket(result);
}

void InventoryManager::syncCraftingPreview(const std::vector<ItemStack> &recipeOutputs,
                                            const std::vector<uint32_t> &recipeSourceIndices) {
    if (mPlayer == nullptr || mSender == nullptr) {
        return;
    }

    const std::vector<ItemStack> &contents = isCraftingTableOpen()
                                             ? mPlayer->getInventory().getCraftingTableContents()
                                             : mPlayer->getInventory().getCraftingContents();
    const int gridWidth = isCraftingTableOpen() ? 3 : 2;
    const ItemStack result = CraftingManager::findResult(contents, gridWidth,
                                                         recipeOutputs, recipeSourceIndices);
    _sendOutputPacket(result);
}

void InventoryManager::onClientSelectHotbarSlot(int slot) {
    mClientSelectedHotbarSlot = slot;
}

void InventoryManager::syncSelectedHotbarSlot() {
    if (mPlayer == nullptr || mSender == nullptr) {
        return;
    }

    const PlayerInventory &inventory = mPlayer->getInventory();
    const int selected = inventory.getSelectedSlot();
    if (selected == mClientSelectedHotbarSlot) {
        return;
    }

    MobEquipmentPacket equipment;
    equipment.mRuntimeActorId = (int64_t) mPlayer->getRuntimeId();
    equipment.mItem = inventory.getItemInHand();
    equipment.mInventorySlot = selected;
    equipment.mHotbarSlot = selected;
    equipment.mContainerId = CONTAINER_ID_INVENTORY;
    mSender->sendPacketTo(mPlayer->getNetworkIdentifier(), equipment);

    mClientSelectedHotbarSlot = selected;
}

void InventoryManager::onCurrentWindowRemove() {
    if (mPlayer == nullptr || mSender == nullptr) {
        return;
    }

    const int currentWindowId = mFurnaceWindowId != CONTAINER_ID_NONE
                                ? mFurnaceWindowId
                                : (mCraftingTableWindowId != CONTAINER_ID_NONE
                                   ? mCraftingTableWindowId
                                   : (mContainerWindowId != CONTAINER_ID_NONE
                                      ? mContainerWindowId
                                      : mMainInventoryWindowId));
    if (currentWindowId == CONTAINER_ID_NONE) {
        return;
    }

    ContainerClosePacket close;
    close.mWindowId = (int8_t) currentWindowId;
    close.mType = mCurrentWindowType;
    close.mServerInitiated = true;
    mSender->sendPacketTo(mPlayer->getNetworkIdentifier(), close);

    mPendingCloseWindowId = currentWindowId;
    mHasPendingCloseWindow = true;
    if (mCraftingTableWindowId == currentWindowId) {
        mCraftingTableWindowId = CONTAINER_ID_NONE;
    } else if (mFurnaceWindowId == currentWindowId) {
        _storeFurnaceState(true);
        mFurnaceWindowId = CONTAINER_ID_NONE;
        mFurnaceBurnTime = 0;
        mFurnaceMaxBurnTime = 0;
        mFurnaceCookTime = 0;
        for (ItemStack &item: mFurnaceObservedItems)
            item = ItemStack::air();
    } else if (mContainerWindowId == currentWindowId) {
        _closeBlockContainer();
        mContainerWindowId = CONTAINER_ID_NONE;
    } else {
        mMainInventoryWindowId = CONTAINER_ID_NONE;
    }
}

bool InventoryManager::_allowPluginOpen(const Vector3i &position) {
    if (mOwner == nullptr || mPlayer == nullptr)
        return true;

    PluginManager &plugins = mOwner->getPluginManager();
    if (!plugins.hasSubscribers(FALCON_EVENT_INVENTORY_OPEN) && !plugins.hasSubscribers(FALCON_EVENT_INVENTORY_CLOSE))
        return true;

    Level &level = mOwner->getLevelFor(*mPlayer);
    PluginEvent openEvent;
    openEvent.mType = FALCON_EVENT_INVENTORY_OPEN;
    openEvent.mCancellable = true;
    openEvent.mPlayer = mPlayer;
    openEvent.mLevel = &level;
    openEvent.mBlockPosition = position;
    openEvent.mBlockName = level.getBlockState(position.x, position.y, position.z).mName;
    plugins.dispatch(openEvent);
    if (openEvent.mCancelled)
        return false;

    mPluginOpen = true;
    mPluginOpenPosition = position;
    return true;
}

void InventoryManager::dispatchPluginClose() {
    if (!mPluginOpen || mOwner == nullptr || mPlayer == nullptr)
        return;

    mPluginOpen = false;
    Level &level = mOwner->getLevelFor(*mPlayer);
    PluginEvent closeEvent;
    closeEvent.mType = FALCON_EVENT_INVENTORY_CLOSE;
    closeEvent.mPlayer = mPlayer;
    closeEvent.mLevel = &level;
    closeEvent.mBlockPosition = mPluginOpenPosition;
    closeEvent.mBlockName = level.getBlockState(mPluginOpenPosition.x, mPluginOpenPosition.y,
                                                mPluginOpenPosition.z).mName;
    mOwner->getPluginManager().dispatch(closeEvent);
}

bool InventoryManager::onClientOpenCraftingTable(const Vector3i &position) {
    if (mPlayer == nullptr || mSender == nullptr) {
        return false;
    }

    if (mCraftingTableWindowId != CONTAINER_ID_NONE) {
        return mCraftingTablePosition == position;
    }

    if (mMainInventoryWindowId != CONTAINER_ID_NONE || mFurnaceWindowId != CONTAINER_ID_NONE || mHasPendingCloseWindow) {
        return false;
    }

    if (!_allowPluginOpen(position))
        return false;

    const int windowId = _getNewWindowId();
    mCraftingTableWindowId = windowId;
    mCraftingTablePosition = position;
    mCurrentWindowType = ContainerType::Workbench;

    ContainerOpenPacket open;
    open.mWindowId = (int8_t) windowId;
    open.mType = ContainerType::Workbench;
    open.mBlockPosition = position;
    open.mUniqueActorId = -1;
    mSender->sendPacketTo(mPlayer->getNetworkIdentifier(), open);
    return true;
}

bool InventoryManager::openContainer(ContainerType type, const Vector3i &position) {
    if (mPlayer == nullptr || mSender == nullptr) {
        return false;
    }

    if (!_allowPluginOpen(position))
        return false;

    ContainerOpenPacket open;
    open.mWindowId = (int8_t) _getNewWindowId();
    open.mType = type;
    open.mBlockPosition = position;
    open.mUniqueActorId = -1;
    mSender->sendPacketTo(mPlayer->getNetworkIdentifier(), open);
    return true;
}

bool InventoryManager::refreshEnchantInput(const ItemStack &current, int32_t newSeed) {
    const std::string identifier = current.isAir() || current.mDefinition == nullptr
                                   ? std::string()
                                   : current.mDefinition->getIdentifier();

    if (identifier == mEnchantInputId && current.mCount == mEnchantInputCount
        && current.mDamage == mEnchantInputDamage)
        return false;

    mEnchantInputId = identifier;
    mEnchantInputCount = current.mCount;
    mEnchantInputDamage = current.mDamage;
    mEnchantSeed = newSeed;
    return true;
}

void InventoryManager::refreshOpenContainer(const Vector3i &position) {
    if (mContainerWindowId == CONTAINER_ID_NONE || !(mContainerPosition == position))
        return;

    Container *container = getContainer();
    if (container != nullptr)
        _sendContentPackets(mContainerWindowId, *container);
}

Container *InventoryManager::getContainer() {
    BlockActorStore *blockActors = _blockActors();
    if (!isContainerOpen() || blockActors == nullptr)
        return nullptr;

    BlockActor *blockActor = blockActors->find(mContainerPosition);
    if (blockActor == nullptr)
        return nullptr;

    EnderChestBlockActor *enderChest = dynamic_cast<EnderChestBlockActor *>(blockActor);
    if (enderChest != nullptr)
        return mPlayer == nullptr ? nullptr : enderChest->getContainerFor(mPlayer->getUniqueId());

    return blockActor->getContainer();
}

bool InventoryManager::onClientOpenBlockContainer(const Vector3i &position, ContainerType type) {
    if (mPlayer == nullptr || mSender == nullptr)
        return false;

    if (mContainerWindowId != CONTAINER_ID_NONE)
        return mContainerPosition == position;

    if (mMainInventoryWindowId != CONTAINER_ID_NONE || mCraftingTableWindowId != CONTAINER_ID_NONE
        || mFurnaceWindowId != CONTAINER_ID_NONE || mHasPendingCloseWindow)
        return false;

    BlockActorStore *blockActors = _blockActors();
    BlockActor *blockActor = blockActors == nullptr ? nullptr : blockActors->find(position);
    if (blockActor == nullptr || !_allowPluginOpen(position))
        return false;

    const int windowId = _getNewWindowId();
    mContainerWindowId = windowId;
    mContainerPosition = position;
    mCurrentWindowType = type;

    BlockActorDataPacket blockActorData;
    blockActorData.mBlockPosition = position;
    blockActorData.mData = blockActor->getSpawnCompound();
    mSender->sendPacketTo(mPlayer->getNetworkIdentifier(), blockActorData);

    ContainerOpenPacket open;
    open.mWindowId = (int8_t) windowId;
    open.mType = type;
    open.mBlockPosition = position;
    open.mUniqueActorId = -1;
    mSender->sendPacketTo(mPlayer->getNetworkIdentifier(), open);

    Container *container = getContainer();
    if (container != nullptr)
        _sendContentPackets(windowId, *container);

    BarrelBlockActor *barrel = dynamic_cast<BarrelBlockActor *>(blockActor);
    if (barrel != nullptr) {
        barrel->addViewer();
        if (barrel->getViewerCount() == 1)
            _animateBarrel(position, true);
    } else if (ShulkerBoxBlockActor *shulkerBox = dynamic_cast<ShulkerBoxBlockActor *>(blockActor)) {
        shulkerBox->addViewer();
        if (shulkerBox->getViewerCount() == 1)
            _animateBlockContainer(position, true, LevelSoundEvent::SHULKER_BOX_OPEN,
                                   LevelSoundEvent::SHULKER_BOX_CLOSED);
    } else if (dynamic_cast<EnderChestBlockActor *>(blockActor) != nullptr) {
        _animateBlockContainer(position, true, LevelSoundEvent::ENDER_CHEST_OPEN,
                               LevelSoundEvent::ENDER_CHEST_CLOSED);
    }

    RedstoneSystem::queueRedstoneNotification(mOwner->getLevelFor(*mPlayer), position);
    return true;
}

bool InventoryManager::onClientOpenChest(const Vector3i &position) {
    if (mPlayer == nullptr || mSender == nullptr)
        return false;

    if (mContainerWindowId != CONTAINER_ID_NONE)
        return mContainerPosition == position;

    if (mMainInventoryWindowId != CONTAINER_ID_NONE || mCraftingTableWindowId != CONTAINER_ID_NONE
        || mFurnaceWindowId != CONTAINER_ID_NONE || mHasPendingCloseWindow)
        return false;

    BlockActorStore *blockActors = _blockActors();
    ChestBlockActor *chest = blockActors == nullptr ? nullptr : blockActors->find<ChestBlockActor>(position);
    if (chest == nullptr || !_allowPluginOpen(position))
        return false;

    const int windowId = _getNewWindowId();
    mContainerWindowId = windowId;
    mContainerPosition = position;
    mCurrentWindowType = ContainerType::Container;

    BlockActorDataPacket blockActorData;
    blockActorData.mBlockPosition = position;
    blockActorData.mData = chest->getSpawnCompound();
    mSender->sendPacketTo(mPlayer->getNetworkIdentifier(), blockActorData);

    ContainerOpenPacket open;
    open.mWindowId = (int8_t) windowId;
    open.mType = ContainerType::Container;
    open.mBlockPosition = chest->getPrimary()->getPosition();
    open.mUniqueActorId = -1;
    mSender->sendPacketTo(mPlayer->getNetworkIdentifier(), open);

    Container *container = getContainer();
    if (container != nullptr)
        _sendContentPackets(windowId, *container);

    chest->addViewer();
    RedstoneSystem::queueRedstoneNotification(mOwner->getLevelFor(*mPlayer), position);

    if (chest->getViewerCount() == 1)
        _animateChest(*chest, true);

    return true;
}

void InventoryManager::_animateChest(ChestBlockActor &chest, bool open) {
    Level *level = chest.getLevel();
    if (mOwner == nullptr || level == nullptr)
        return;

    const Vector3i &position = chest.getPosition();
    const Vector3f center((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);

    BlockEventPacket event;
    event.mBlockPosition = position;
    event.mEventType = CHEST_ANIMATION_EVENT_TYPE;
    event.mEventData = open ? 1 : 0;
    BlockActionHandler::broadcastToViewers(*mOwner, *level, center, event);

    mOwner->playLevelSound(*level, open ? LevelSoundEvent::CHEST_OPEN : LevelSoundEvent::CHEST_CLOSED, center);

    ChestBlockActor *pair = chest.getPair();
    if (pair == nullptr)
        return;

    const Vector3i &pairPosition = pair->getPosition();

    BlockEventPacket pairEvent;
    pairEvent.mBlockPosition = pairPosition;
    pairEvent.mEventType = CHEST_ANIMATION_EVENT_TYPE;
    pairEvent.mEventData = open ? 1 : 0;
    BlockActionHandler::broadcastToViewers(*mOwner, *level,
                                           Vector3f((float) pairPosition.x + 0.5f,
                                                    (float) pairPosition.y + 0.5f,
                                                    (float) pairPosition.z + 0.5f),
                                           pairEvent);
}

void InventoryManager::_animateBlockContainer(const Vector3i &position, bool open, const char *openSound,
                                               const char *closeSound) {
    if (mOwner == nullptr || mPlayer == nullptr)
        return;

    Level &level = mOwner->getLevelFor(*mPlayer);
    const Vector3f center((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);

    BlockEventPacket event;
    event.mBlockPosition = position;
    event.mEventType = CHEST_ANIMATION_EVENT_TYPE;
    event.mEventData = open ? 2 : 0;
    BlockActionHandler::broadcastToViewers(*mOwner, level, center, event);
    mOwner->playLevelSound(level, open ? openSound : closeSound, center);
}

void InventoryManager::_animateBarrel(const Vector3i &position, bool open) {
    if (mOwner == nullptr || mPlayer == nullptr)
        return;

    Level &level = mOwner->getLevelFor(*mPlayer);
    const BlockState state = level.getBlockState(position.x, position.y, position.z);
    if (state.mName != "minecraft:barrel")
        return;

    const Tag *current = state.mStates.get("open_bit");
    const bool currentlyOpen = current != nullptr && current->getType() == Tag::Type::Byte
                               && current->asByte() != 0;
    if (currentlyOpen == open)
        return;

    Tag states = state.mStates;
    states.putByte("open_bit", open ? 1 : 0);
    level.setBlock(position, BlockState(state.mName, states), false);

    const Vector3f center((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    mOwner->playLevelSound(level, open ? LevelSoundEvent::BARREL_OPEN : LevelSoundEvent::BARREL_CLOSED, center);
}

void InventoryManager::_closeBlockContainer() {
    BlockActorStore *blockActors = _blockActors();
    BlockActor *blockActor = blockActors == nullptr ? nullptr : blockActors->find(mContainerPosition);
    if (blockActor == nullptr)
        return;

    ChestBlockActor *chest = dynamic_cast<ChestBlockActor *>(blockActor);
    if (chest != nullptr) {
        const bool wasLastViewer = chest->getViewerCount() == 1;
        chest->removeViewer();
        RedstoneSystem::queueRedstoneNotification(mOwner->getLevelFor(*mPlayer), mContainerPosition);
        if (wasLastViewer)
            _animateChest(*chest, false);
        return;
    }

    BarrelBlockActor *barrel = dynamic_cast<BarrelBlockActor *>(blockActor);
    if (barrel != nullptr) {
        const bool wasLastViewer = barrel->getViewerCount() == 1;
        barrel->removeViewer();
        if (wasLastViewer)
            _animateBarrel(mContainerPosition, false);
        return;
    }

    ShulkerBoxBlockActor *shulkerBox = dynamic_cast<ShulkerBoxBlockActor *>(blockActor);
    if (shulkerBox != nullptr) {
        const bool wasLastViewer = shulkerBox->getViewerCount() == 1;
        shulkerBox->removeViewer();
        if (wasLastViewer)
            _animateBlockContainer(mContainerPosition, false, LevelSoundEvent::SHULKER_BOX_OPEN,
                                   LevelSoundEvent::SHULKER_BOX_CLOSED);
        return;
    }

    if (dynamic_cast<EnderChestBlockActor *>(blockActor) != nullptr)
        _animateBlockContainer(mContainerPosition, false, LevelSoundEvent::ENDER_CHEST_OPEN,
                               LevelSoundEvent::ENDER_CHEST_CLOSED);
}

bool InventoryManager::onClientOpenFurnace(const Vector3i &position, FurnaceKind kind, ContainerType type) {
    if (mPlayer == nullptr || mSender == nullptr) {
        return false;
    }
    if (mFurnaceWindowId != CONTAINER_ID_NONE) {
        return mFurnacePosition == position;
    }
    if (mMainInventoryWindowId != CONTAINER_ID_NONE || mCraftingTableWindowId != CONTAINER_ID_NONE
        || mHasPendingCloseWindow) {
        return false;
    }

    BlockActorStore *blockActors = _blockActors();
    if (blockActors == nullptr || !_allowPluginOpen(position))
        return false;

    const int windowId = _getNewWindowId();
    mFurnaceWindowId = windowId;
    mFurnacePosition = position;
    mFurnaceKind = kind;
    mFurnaceContainerType = type;
    mFurnaceBurnTime = 0;
    mFurnaceMaxBurnTime = 0;
    mFurnaceCookTime = 0;
    FurnaceBlockActor &state = blockActors->getOrCreate<FurnaceBlockActor>(position);
    state.mKind = kind;

    BlockActorDataPacket blockActorData;
    blockActorData.mBlockPosition = position;
    blockActorData.mData = state.getSpawnCompound();
    mSender->sendPacketTo(mPlayer->getNetworkIdentifier(), blockActorData);

    mFurnaceBurnTime = state.mBurnTime;
    mFurnaceMaxBurnTime = state.mMaxBurnTime;
    mFurnaceCookTime = state.mCookTime;
    for (int slot = 0; slot < FurnaceInventory::SIZE; ++slot) {
        mPlayer->getInventory().setFurnaceItem(slot, state.mInventory.mItems[(size_t) slot]);
        mFurnaceObservedItems[(size_t) slot] = state.mInventory.mItems[(size_t) slot];
    }
    mCurrentWindowType = type;

    ContainerOpenPacket open;
    open.mWindowId = (int8_t) windowId;
    open.mType = type;
    open.mBlockPosition = position;
    open.mUniqueActorId = -1;
    mSender->sendPacketTo(mPlayer->getNetworkIdentifier(), open);
    syncContents(InventoryId::FurnaceInput);
    return true;
}

void InventoryManager::onClientOpenMainInventory() {
    if (mPlayer == nullptr || mSender == nullptr) {
        return;
    }

    if (mMainInventoryWindowId != CONTAINER_ID_NONE) {
        return;
    }

    if (mCraftingTableWindowId != CONTAINER_ID_NONE || mContainerWindowId != CONTAINER_ID_NONE
        || mFurnaceWindowId != CONTAINER_ID_NONE) {
        mHasPendingOpenMainInventory = true;
        onCurrentWindowRemove();
        return;
    }

    if (mHasPendingCloseWindow) {
        mHasPendingOpenMainInventory = true;
        return;
    }

    const int windowId = _getNewWindowId();
    mMainInventoryWindowId = windowId;
    mCurrentWindowType = ContainerType::Inventory;

    ContainerOpenPacket open;
    open.mWindowId = (int8_t) windowId;
    open.mType = mCurrentWindowType;
    open.mBlockPosition = Vector3i(0, 0, 0);
    open.mUniqueActorId = mPlayer->getUniqueId();
    mSender->sendPacketTo(mPlayer->getNetworkIdentifier(), open);
}

void InventoryManager::onClientRemoveWindow(int windowId) {
    if (mPlayer == nullptr || mSender == nullptr) {
        return;
    }

    if (windowId == CONTAINER_ID_NONE) {
        windowId = mLastInventoryNetworkId;
    }

    if (windowId == mMainInventoryWindowId) {
        mMainInventoryWindowId = CONTAINER_ID_NONE;
    }

    if (windowId == mCraftingTableWindowId) {
        mCraftingTableWindowId = CONTAINER_ID_NONE;
    }

    if (windowId == mContainerWindowId) {
        _closeBlockContainer();
        mContainerWindowId = CONTAINER_ID_NONE;
    }
    if (windowId == mFurnaceWindowId) {
        _storeFurnaceState(true);
        mFurnaceWindowId = CONTAINER_ID_NONE;
        mFurnaceBurnTime = 0;
        mFurnaceMaxBurnTime = 0;
        mFurnaceCookTime = 0;
        for (ItemStack &item: mFurnaceObservedItems)
            item = ItemStack::air();
    }

    ContainerClosePacket close;
    close.mWindowId = (int8_t) windowId;
    close.mType = mCurrentWindowType;
    close.mServerInitiated = false;
    mSender->sendPacketTo(mPlayer->getNetworkIdentifier(), close);

    if (mHasPendingCloseWindow && mPendingCloseWindowId == windowId) {
        mHasPendingCloseWindow = false;
        mPendingCloseWindowId = CONTAINER_ID_NONE;

        if (mHasPendingOpenMainInventory) {
            mHasPendingOpenMainInventory = false;
            onClientOpenMainInventory();
        }
    }
}
