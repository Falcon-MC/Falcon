#include "Network/Handler/InventoryHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/ContainerClosePacket.h"
#include "Protocol/Packets/ContainerOpenPacket.h"
#include "Protocol/Packets/InteractPacket.h"
#include "Protocol/Packets/InventoryContentPacket.h"
#include "Protocol/Packets/ItemStackRequestPacket.h"
#include "Protocol/Packets/ItemStackResponsePacket.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/RideSystem.h"
#include "World/Inventory/ItemStackRequestHandler.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Mount inventories", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getOverworld();
        ContainerOpenPacket mOpened;
        std::vector<InventoryContentPacket> mContents;
        std::vector<NetworkIdentifier> mContentRecipients;
        std::vector<ItemStackResponseEntry> mResponses;

        Arena() {
            require(mLevel.openStorage(ServerPaths::getRoot().string()), "could not open mount storage");
            for (int x = -1; x <= 1; ++x) {
                for (int z = -1; z <= 1; ++z)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            mOwner.getNetworkHandler().setOutboundFilter([this](const NetworkIdentifier &id, std::string &bytes) {
                ReadOnlyBinaryStream header(bytes);
                unsigned char sender = 0, recipient = 0;
                const auto type = Packet::peekId(header, sender, recipient);
                ReadOnlyBinaryStream body(bytes);
                if (type == MinecraftPacketIds::ContainerOpen) {
                    mOpened.readHeader(body);
                    mOpened.read(body, mOwner.getCodecContext());
                } else if (type == MinecraftPacketIds::InventoryContent) {
                    InventoryContentPacket packet;
                    packet.readHeader(body);
                    packet.read(body, mOwner.getCodecContext());
                    mContents.push_back(std::move(packet));
                    mContentRecipients.push_back(id);
                } else if (type == MinecraftPacketIds::ItemStackResponse) {
                    ItemStackResponsePacket packet;
                    packet.readHeader(body);
                    packet.read(body, mOwner.getCodecContext());
                    mResponses = std::move(packet.mEntries);
                }
                return false;
            });
        }

        ~Arena() {
            mOwner.getNetworkHandler().setOutboundFilter(nullptr);
        }

        ServerPlayer &player(uint64_t runtimeId) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            auto &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(mLevel);
            player.setPosition(Vector3f(1.5f, 100.0f, 0.5f));
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.getInventoryManager().attach(&player, &mOwner);
            return player;
        }

        MobActor &spawn(const char *identifier, ServerPlayer &player) {
            auto *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, Vector3f(0.5f, 100.0f, 0.5f),
                    [](ServerActor &actor) {
                        static_cast<MobActor &>(actor).setSpawnEvent("minecraft:spawn_adult");
                    }));
            require(mob != nullptr, "mount did not spawn");
            mob->startDefinition(mOwner);
            require(mob->getEntityContainer() == nullptr, "wild mount exposed an inventory");
            require(mob->tameTo(mOwner, player, "minecraft:tamemount", false), "mount could not be tamed");
            require(mob->getEntityContainer() != nullptr, "tamed mount has no inventory");
            return *mob;
        }

        void open(ServerPlayer &player, MobActor &mob) {
            player.getFlags().set(ActorFlag::Sneaking, true);
            require(InventoryHandler::interactWithEntity(mOwner, player, mob.getRuntimeId(), Vector3f()),
                    "sneaking interaction did not open mount");
            player.getFlags().set(ActorFlag::Sneaking, false);
            require(mOpened.mType == ContainerType::Horse && mOpened.mUniqueActorId == mob.getUniqueId(),
                    "open packet names the wrong mount or screen");
            require(!mContents.empty() && mContents.back().mContainerId == mOpened.mWindowId
                    && mContents.back().mContents.size() == (size_t) mob.getEntityContainer()->getContainerSize(),
                    "open packet did not include the full inventory");
        }

        void close(ServerPlayer &player) {
            ContainerClosePacket packet;
            packet.mWindowId = (int8_t) player.getInventoryManager().getCurrentWindowId();
            packet.mType = ContainerType::Horse;
            InventoryHandler::handleContainerClose(player, packet);
            require(player.getInventoryManager().getContainer() == nullptr, "closed mount is still accessible");
        }

        ItemStackRequestAction move(ServerPlayer &player, Container &container, int source, ContainerSlotType type,
                                    int destination, int count, bool take = false) {
            ItemStackRequestAction action;
            action.mType = take ? ItemStackRequestActionType::Take : ItemStackRequestActionType::Place;
            action.mCount = count;
            action.mSource.mContainer = action.mSource.mContainerName.mContainer = ContainerSlotType::Inventory;
            action.mSource.mSlot = source;
            action.mSource.mStackNetworkId = player.getInventory().getItem(source).mNetId;
            action.mDestination.mContainer = action.mDestination.mContainerName.mContainer = type;
            action.mDestination.mSlot = destination;
            const int physical = destination + (type == ContainerSlotType::LevelEntity ? container.getEquipmentSlotCount() : 0);
            action.mDestination.mStackNetworkId = physical < container.getContainerSize()
                    ? container.getContainerItem(physical).mNetId : 0;
            if (take)
                std::swap(action.mSource, action.mDestination);
            return action;
        }

        void send(ServerPlayer &player, std::vector<ItemStackRequestAction> actions, bool expected) {
            ItemStackRequestPacket packet;
            packet.mRequests.resize(1);
            packet.mRequests[0].mRequestId = -1;
            packet.mRequests[0].mActions = std::move(actions);
            // exercise the wire layout as well as the handler
            BinaryStream encoded;
            packet.write(encoded, mOwner.getCodecContext());
            ReadOnlyBinaryStream stream(encoded.getBuffer());
            ItemStackRequestPacket decoded;
            decoded.read(stream, mOwner.getCodecContext());
            mResponses.clear();
            InventoryHandler::handleItemStackRequest(mOwner, player.getNetworkIdentifier(), player, decoded);
            require(mResponses.size() == 1 && (mResponses[0].mResult == ItemStackRequestHandler::RESULT_OK) == expected,
                    "unexpected stack request response for " + std::to_string(player.getRuntimeId())
                    + " source=" + std::to_string((int) decoded.mRequests[0].mActions[0].mSource.mContainerName.mContainer)
                    + ":" + std::to_string(decoded.mRequests[0].mActions[0].mSource.mSlot)
                    + " destination=" + std::to_string((int) decoded.mRequests[0].mActions[0].mDestination.mContainerName.mContainer)
                    + ":" + std::to_string(decoded.mRequests[0].mActions[0].mDestination.mSlot)
                    + " expected=" + std::to_string(expected));
        }
    };

    void horse(Arena &arena) {
        auto &player = arena.player(5001);
        auto &horse = arena.spawn("minecraft:horse", player);
        auto &container = *horse.getEntityContainer();
        require(container.getContainerSize() == 2 && container.getEquipmentSlotCount() == 2, "horse slot count");
        arena.open(player, horse);
        player.getInventory().setItem(0, arena.mOwner.createItemStack("minecraft:saddle", 1));
        player.getInventory().setItem(1, arena.mOwner.createItemStack("minecraft:diamond_horse_armor", 1));
        arena.send(player, {arena.move(player, container, 0, ContainerSlotType::HorseEquip, 0, 1),
                            arena.move(player, container, 1, ContainerSlotType::HorseEquip, 1, 1)}, true);
        require(horse.getComponent("minecraft:is_saddled") != nullptr, "saddle did not enable riding control");
        require(horse.getEquipment().getSlot(MobEquipment::BODY).mDefinition->getIdentifier() == "minecraft:diamond_horse_armor",
                "armor was not equipped on the body");
        player.getInventory().setItem(2, arena.mOwner.createItemStack("minecraft:stone", 1));
        arena.send(player, {arena.move(player, container, 2, ContainerSlotType::LevelEntity, 0, 1)}, false);
        arena.send(player, {arena.move(player, container, 2, ContainerSlotType::HorseEquip, 1, 1)}, false);
        arena.send(player, {arena.move(player, container, 3, ContainerSlotType::HorseEquip, 0, 1, true)}, true);
        require(horse.getComponent("minecraft:is_saddled") == nullptr, "removed saddle left riding control enabled");
        arena.close(player);
        require(RideSystem::mount(arena.mOwner, player, horse, true), "player could not mount horse");
        InteractPacket packet;
        packet.mAction = InteractPacket::Action::OpenInventory;
        packet.mRuntimeActorId = player.getRuntimeId();
        packet.handle(player.getNetworkIdentifier(), arena.mOwner);
        require(player.getInventoryManager().getContainer() == &container, "inventory key did not open ridden horse");
        arena.close(player);
        RideSystem::dismount(arena.mOwner, player, true);
        const int64_t id = horse.getUniqueId();
        arena.mOwner.saveActorsForChunk(arena.mLevel, 0, 0, true);
        arena.mOwner.loadActorsForChunk(arena.mLevel, 0, 0);
        auto *restored = dynamic_cast<MobActor *>(arena.mOwner.getActor(id));
        require(restored != nullptr, "saved horse was not restored");
        require(restored->getEntityContainer() != nullptr, "saved horse lost its window");
        require(restored->getEntityContainer()->getContainerItem(0).isAir(), "saved horse restored a removed saddle");
        require(!restored->getEquipment().getSlot(MobEquipment::BODY).isAir()
                && restored->getEquipment().getSlot(MobEquipment::BODY).mDefinition->getIdentifier()
                        == "minecraft:diamond_horse_armor", "saved horse lost its armor");
    }

    void llama(Arena &arena, int strength) {
        auto &player = arena.player(5100 + strength);
        auto &llama = arena.spawn("minecraft:llama", player);
        for (int value = 1; value <= 5; ++value)
            llama.removeComponentGroup("minecraft:strength_" + std::to_string(value));
        llama.addComponentGroup("minecraft:strength_" + std::to_string(strength));
        auto &container = *llama.getEntityContainer();
        require(container.getContainerSize() == 1, "unchested llama exposed cargo");
        const char *colors[] = {"white", "orange", "magenta", "light_blue", "yellow", "lime", "pink", "gray",
                                "light_gray", "cyan", "purple", "blue", "brown", "green", "red", "black"};
        for (const char *color: colors)
            require(container.canHoldInSlot(0, arena.mOwner.createItemStack(std::string("minecraft:") + color + "_carpet", 1)),
                    "llama rejected a dyed carpet");
        require(!container.canHoldInSlot(0, arena.mOwner.createItemStack("minecraft:moss_carpet", 1)),
                "llama accepted moss as a dyed carpet");
        arena.open(player, llama);
        player.getInventory().setItem(0, arena.mOwner.createItemStack("minecraft:red_carpet", 8));
        arena.send(player, {arena.move(player, container, 0, ContainerSlotType::HorseEquip, 0, 2)}, false);
        arena.send(player, {arena.move(player, container, 0, ContainerSlotType::HorseEquip, 0, 1)}, true);
        require(player.getInventory().getItem(0).mCount == 7 && container.getContainerItem(0).mCount == 1,
                "equipping one carpet lost the rest of the stack");
        arena.send(player, {arena.move(player, container, 0, ContainerSlotType::HorseEquip, 0, 1)}, false);
        arena.send(player, {arena.move(player, container, 0, ContainerSlotType::LevelEntity, 0, 1)}, false);
        player.getInventory().setItem(5, arena.mOwner.createItemStack("minecraft:blue_carpet", 2));
        auto swap = arena.move(player, container, 5, ContainerSlotType::HorseEquip, 0, 1);
        swap.mType = ItemStackRequestActionType::Swap;
        arena.send(player, {swap}, false);
        player.getInventory().setItem(5, arena.mOwner.createItemStack("minecraft:blue_carpet", 1));
        for (int pass = 0; pass < 2; ++pass) {
            swap = arena.move(player, container, 5, ContainerSlotType::HorseEquip, 0, 1);
            swap.mType = ItemStackRequestActionType::Swap;
            arena.send(player, {swap}, true);
        }
        require(container.getContainerItem(0).mDefinition->getIdentifier() == "minecraft:red_carpet",
                "carpet swaps lost the original equipment");
        arena.close(player);
        player.getInventory().setSelectedSlot(1);
        player.getInventory().setItem(1, arena.mOwner.createItemStack("minecraft:chest", 1));
        require(InventoryHandler::interactWithEntity(arena.mOwner, player, llama.getRuntimeId(), Vector3f()),
                "chest interaction was ignored");
        require(llama.getComponent("minecraft:is_chested") != nullptr && player.getInventory().getItem(1).isAir(),
                "chest was not attached and consumed");
        require(container.getContainerSize() == 1 + 3 * strength, "llama capacity does not follow strength");
        EntityDataMap metadata;
        llama.fillSpawnMetadata(metadata);
        bool foundStrength = false, foundModifier = false;
        for (const auto &entry: metadata.mEntries) {
            if (entry.mId == ActorFlags::STRENGTH_DATA_ID)
                foundStrength = entry.mFormat == EntityDataFormat::Int && entry.mIntValue == strength;
            if (entry.mId == ActorFlags::CONTAINER_STRENGTH_DATA_ID)
                foundModifier = entry.mFormat == EntityDataFormat::Int && entry.mIntValue == 3;
        }
        require(foundStrength && foundModifier, "client cannot compute llama cargo size");
        arena.open(player, llama);
        auto &viewer = arena.player(5200 + strength);
        arena.open(viewer, llama);
        arena.mContentRecipients.clear();
        player.getInventory().setItem(2, arena.mOwner.createItemStack("minecraft:diamond", 32));
        const int last = strength * 3 - 1;
        arena.send(player, {arena.move(player, container, 2, ContainerSlotType::LevelEntity, 0, 12)}, true);
        require(std::find(arena.mContentRecipients.begin(), arena.mContentRecipients.end(), viewer.getNetworkIdentifier())
                        != arena.mContentRecipients.end(), "other viewer did not receive the changed cargo");
        arena.close(viewer);
        arena.send(player, {arena.move(player, container, 2, ContainerSlotType::LevelEntity, last, 20)}, true);
        require(container.getContainerItem(0).mDefinition->getIdentifier() == "minecraft:red_carpet"
                && container.getContainerItem(1).mCount == 12 && container.getContainerItem(last + 1).mCount == 20,
                "cargo overwrote equipment or used the wrong slot");
        arena.send(player, {arena.move(player, container, 3, ContainerSlotType::LevelEntity, last, 5, true)}, true);
        require(player.getInventory().getItem(3).mCount == 5 && container.getContainerItem(last + 1).mCount == 15,
                "cargo split lost items");
        // the first action must roll back when the second names a nonexistent slot
        arena.send(player, {arena.move(player, container, 3, ContainerSlotType::LevelEntity, 0, 1),
                            arena.move(player, container, 3, ContainerSlotType::LevelEntity, last + 1, 1)}, false);
        require(player.getInventory().getItem(3).mCount == 5 && container.getContainerItem(1).mCount == 12,
                "invalid multi-action request partly committed");
        player.setPosition(Vector3f(100.0f, 100.0f, 0.5f));
        arena.send(player, {arena.move(player, container, 3, ContainerSlotType::LevelEntity, 0, 1)}, false);
        require(player.getInventory().getItem(3).mCount == 5, "distant player moved cargo");
        player.setPosition(Vector3f(1.5f, 100.0f, 0.5f));
        arena.close(player);
        const int64_t id = llama.getUniqueId();
        arena.mOwner.saveActorsForChunk(arena.mLevel, 0, 0, true);
        require(arena.mOwner.getActor(id) == nullptr, "llama was not unloaded");
        arena.mOwner.loadActorsForChunk(arena.mLevel, 0, 0);
        auto *restored = dynamic_cast<MobActor *>(arena.mOwner.getActor(id));
        require(restored != nullptr && restored->getInventoryStrength() == strength, "saved llama lost its strength");
        auto *saved = restored->getEntityContainer();
        require(saved != nullptr && saved->getContainerSize() == 1 + 3 * strength
                && saved->getContainerItem(0).mDefinition->getIdentifier() == "minecraft:red_carpet"
                && saved->getContainerItem(1).mCount == 12 && saved->getContainerItem(last + 1).mCount == 15,
                "saved llama lost equipment, chest or cargo");
        arena.open(player, *restored);
        arena.close(player);
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
            / ("falcon-mount-inventory-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        Arena arena;
        horse(arena);
        for (int strength = 1; strength <= 5; ++strength)
            llama(arena, strength);
        std::cout << "mount inventory checks passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
