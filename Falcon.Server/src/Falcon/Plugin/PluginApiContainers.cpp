#include "Falcon/Plugin/PluginServerApi.h"

#include "Falcon/Plugin/PluginApiHelpers.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/Container.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/SignBlockActor.h"
#include "World/Level/Block/Inventory/EnderChestInventoryStore.h"
#include "World/Level/Block/Inventory/SimpleContainerInventory.h"
#include "World/Level/Level.h"

#include <memory>
#include <string>
#include <utility>

using namespace PluginApiHelpers;

namespace {
    Vector3i toVector3i(const FalconBlockPos &value) {
        return Vector3i(value.x, value.y, value.z);
    }

    Container *containerAt(FalconLevel *handle, FalconBlockPos position) {
        BlockActor *blockActor = level(handle)->getBlockActors().find(toVector3i(position));
        return blockActor == nullptr ? nullptr : blockActor->getContainer();
    }

    uint32_t levelContainerSize(FalconLevel *handle, FalconBlockPos position) {
        const Container *container = containerAt(handle, position);
        return container == nullptr ? 0 : (uint32_t) container->getContainerSize();
    }

    FalconItem *levelContainerItem(FalconLevel *handle, FalconBlockPos position, uint32_t slot) {
        const Container *container = containerAt(handle, position);
        if (container == nullptr || slot >= (uint32_t) container->getContainerSize())
            return nullptr;
        return copyOf(container->getContainerItem((int) slot));
    }

    int levelSetContainerItem(FalconLevel *handle, FalconBlockPos position, uint32_t slot, FalconItem *stack) {
        Container *container = containerAt(handle, position);
        if (container == nullptr || slot >= (uint32_t) container->getContainerSize()
            || container->isSlotDisabled((int) slot))
            return 0;

        ItemStack value = valueOf(stack);
        if (!value.isAir() && !container->canHold(value))
            return 0;

        container->setContainerItem((int) slot, std::move(value));
        owner().refreshContainerViewers(toVector3i(position));
        return 1;
    }

    const char *sideKey(FalconSignSide side) {
        if (side == FALCON_SIGN_FRONT)
            return "FrontText";
        return side == FALCON_SIGN_BACK ? "BackText" : nullptr;
    }

    const char *levelSignText(FalconLevel *handle, FalconBlockPos position, FalconSignSide side) {
        const char *key = sideKey(side);
        SignBlockActor *sign = level(handle)->getBlockActors().find<SignBlockActor>(toVector3i(position));
        if (key == nullptr || sign == nullptr)
            return nullptr;

        const Tag saved = sign->saveNbt();
        const Tag *face = saved.get(key);
        return hold(face != nullptr && face->isCompound() ? face->getString("Text") : std::string());
    }

    // the sign's own tag is written back whole, like a player's edit
    int levelSetSignText(FalconLevel *handle, FalconBlockPos position, FalconSignSide side, const char *text) {
        const char *key = sideKey(side);
        Level &target = *level(handle);
        SignBlockActor *sign = target.getBlockActors().find<SignBlockActor>(toVector3i(position));
        if (key == nullptr || sign == nullptr || text == nullptr)
            return 0;

        Tag data = sign->saveNbt();
        Tag face = data.get(key) != nullptr ? *data.get(key) : Tag::ofCompound();
        face.putString("Text", text);
        data.put(key, face);
        sign->loadNbt(data, owner().getCodecContext());
        BlockActionHandler::broadcastBlockActorData(owner(), target, *sign);
        return 1;
    }

    SimpleContainerInventory &enderChestOf(FalconPlayer *target) {
        return EnderChestInventoryStore::getInstance().get(player(target)->getUniqueId());
    }

    uint32_t playerEnderChestSize(FalconPlayer *target) {
        return (uint32_t) enderChestOf(target).getContainerSize();
    }

    FalconItem *playerEnderChestItem(FalconPlayer *target, uint32_t slot) {
        const SimpleContainerInventory &chest = enderChestOf(target);
        if (slot >= (uint32_t) chest.getContainerSize())
            return nullptr;
        return copyOf(chest.getContainerItem((int) slot));
    }

    int playerSetEnderChestItem(FalconPlayer *target, uint32_t slot, FalconItem *stack) {
        SimpleContainerInventory &chest = enderChestOf(target);
        if (slot >= (uint32_t) chest.getContainerSize())
            return 0;
        chest.setContainerItem((int) slot, valueOf(stack));
        return 1;
    }
}

namespace {
    uint32_t playerOpenWindow(FalconPlugin *source, FalconPlayer *target, const FalconWindowDescriptor *descriptor) {
        if (source == nullptr || target == nullptr || !plugin(source)->mEnabled || descriptor == nullptr || descriptor->size == 0
            || descriptor->size > 255)
            return 0;
        InventoryManager &inventories = player(target)->getInventoryManager();
        LoadedPlugin &owner = *plugin(source);
        const uint64_t playerId = player(target)->getRuntimeId();
        if (!owner.mWindowPlayers.insert(playerId).second)
            return 0;

        const FalconWindowCloseHandler onClose = descriptor->onClose;
        void *userData = descriptor->userData;
        const int opened = inventories.openPluginWindow(
                std::make_shared<SimpleContainerInventory>((int) descriptor->size),
                (ContainerType) descriptor->containerType, toVector3i(descriptor->position), descriptor->actorUniqueId,
                [&owner, playerId, target, onClose, userData](int windowId) {
                    owner.mWindowPlayers.erase(playerId);
                    if (onClose != nullptr)
                        onClose(target, (uint32_t) windowId, userData);
                });
        if (opened == InventoryManager::CONTAINER_ID_NONE) {
            owner.mWindowPlayers.erase(playerId);
            return 0;
        }
        return (uint32_t) opened;
    }

    FalconItem *playerWindowItem(FalconPlayer *target, uint32_t window, uint32_t slot) {
        Container *container = player(target)->getInventoryManager().getPluginWindow((int) window);
        if (container == nullptr || slot >= (uint32_t) container->getContainerSize())
            return nullptr;
        return copyOf(container->getContainerItem((int) slot));
    }

    int playerSetWindowItem(FalconPlayer *target, uint32_t window, uint32_t slot, FalconItem *stack) {
        InventoryManager &inventories = player(target)->getInventoryManager();
        Container *container = inventories.getPluginWindow((int) window);
        if (container == nullptr || slot >= (uint32_t) container->getContainerSize())
            return 0;
        container->setContainerItem((int) slot, valueOf(stack));
        inventories.refreshPluginWindow();
        return 1;
    }

    void playerCloseWindow(FalconPlayer *target, uint32_t window) {
        InventoryManager &inventories = player(target)->getInventoryManager();
        if (inventories.getPluginWindow((int) window) != nullptr)
            inventories.onCurrentWindowRemove();
    }
}

void PluginServerApi::fillContainers(FalconServerApi &api) {
    api.playerOpenWindow = &playerOpenWindow;
    api.playerWindowItem = &playerWindowItem;
    api.playerSetWindowItem = &playerSetWindowItem;
    api.playerCloseWindow = &playerCloseWindow;
    api.levelContainerSize = &levelContainerSize;
    api.levelContainerItem = &levelContainerItem;
    api.levelSetContainerItem = &levelSetContainerItem;
    api.levelSignText = &levelSignText;
    api.levelSetSignText = &levelSetSignText;
    api.playerEnderChestSize = &playerEnderChestSize;
    api.playerEnderChestItem = &playerEnderChestItem;
    api.playerSetEnderChestItem = &playerSetEnderChestItem;
}
