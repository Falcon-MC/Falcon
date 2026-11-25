#include "Network/Handler/InventoryHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Packets/InventoryTransactionPacket.h"
#include "Protocol/Packets/SetActorDataPacket.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/BreedGoal.h"
#include "World/Actor/AI/Goal/OwnerTargetGoal.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    const int32_t RED = 14;
    const int32_t ORANGE = 1;
    const int32_t YELLOW = 4;
    const int32_t LIME = 5;
    const int32_t BLUE = 11;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Taming", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getOverworld();
        std::vector<ActorEventPacket> mEvents;
        std::vector<SetActorDataPacket> mData;

        Arena() {
            require(mLevel.openStorage(ServerPaths::getRoot().string()), "could not open taming storage");
            for (int x = -1; x <= 1; ++x) {
                for (int z = -1; z <= 1; ++z)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            mOwner.getNetworkHandler().setOutboundFilter([this](const NetworkIdentifier &, std::string &bytes) {
                ReadOnlyBinaryStream header(bytes);
                unsigned char sender = 0, recipient = 0;
                const auto type = Packet::peekId(header, sender, recipient);
                ReadOnlyBinaryStream body(bytes);
                if (type == MinecraftPacketIds::ActorEvent) {
                    ActorEventPacket packet;
                    packet.readHeader(body);
                    packet.read(body, mOwner.getCodecContext());
                    mEvents.push_back(packet);
                } else if (type == MinecraftPacketIds::SetActorData) {
                    SetActorDataPacket packet;
                    packet.readHeader(body);
                    packet.read(body, mOwner.getCodecContext());
                    mData.push_back(std::move(packet));
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
            player.setGameType((int32_t) GameType::Survival);
            player.getInventoryManager().attach(&player, &mOwner);
            return player;
        }

        MobActor &spawn(const char *identifier, const char *event) {
            auto *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, Vector3f(0.5f, 100.0f, 0.5f),
                    [event](ServerActor &actor) {
                        static_cast<MobActor &>(actor).setSpawnEvent(event);
                    }));
            require(mob != nullptr, std::string(identifier) + " did not spawn");
            mob->startDefinition(mOwner);
            return *mob;
        }

        void hold(ServerPlayer &player, const char *item, int count) {
            player.getInventory().setSelectedSlot(0);
            player.getInventory().setItem(0, item == nullptr ? ItemStack::air() : mOwner.createItemStack(item, count));
        }

        int held(ServerPlayer &player) {
            const ItemStack &stack = player.getInventory().getItemInHand();
            return stack.isAir() ? 0 : stack.mCount;
        }

        // the use-on-entity transaction the client sends, through the wire format and the handler
        void interact(ServerPlayer &player, MobActor &mob) {
            InventoryTransactionPacket packet;
            packet.mTransactionType = InventoryTransactionType::ItemUseOnEntity;
            packet.mActionType = 0;
            packet.mRuntimeActorId = (int64_t) mob.getRuntimeId();
            packet.mHotbarSlot = player.getInventory().getSelectedSlot();
            packet.mItemInHand = player.getInventory().getItemInHand();
            packet.mPlayerPosition = player.getPosition();
            BinaryStream encoded;
            packet.write(encoded, mOwner.getCodecContext());
            ReadOnlyBinaryStream stream(encoded.getBuffer());
            InventoryTransactionPacket decoded;
            decoded.read(stream, mOwner.getCodecContext());
            InventoryHandler::handleTransaction(mOwner, player, decoded);
        }

        int events(const MobActor &mob, EntityEventType type) const {
            int count = 0;
            for (const ActorEventPacket &event: mEvents) {
                if (event.mRuntimeActorId == mob.getRuntimeId() && event.mEventId == (uint8_t) type)
                    ++count;
            }
            return count;
        }

        // the last color the viewers were sent for the mob, -1 for none
        int32_t sentColor(const MobActor &mob) const {
            int32_t color = -1;
            for (const SetActorDataPacket &packet: mData) {
                if ((uint64_t) packet.mRuntimeActorId != mob.getRuntimeId())
                    continue;
                for (const EntityDataEntry &entry: packet.mMetadata.mEntries) {
                    if (entry.mId == ActorFlags::COLOR_DATA_ID)
                        color = entry.mByteValue;
                }
            }
            return color;
        }

        // feeds the tame item until a roll succeeds: each failure is one smoke event, the success one heart event
        void tame(ServerPlayer &player, MobActor &mob, const char *item) {
            hold(player, item, 64);
            int attempts = 0;
            while (!mob.isTamed() && attempts < 64) {
                interact(player, mob);
                ++attempts;
            }
            require(mob.isTamed() && mob.isOwnedBy(player), std::string(item) + " never tamed the mob");
            require(held(player) == 64 - attempts, "each taming attempt did not take one item");
            require(events(mob, EntityEventType::TamingFailed) == attempts - 1, "failed attempts did not smoke");
            require(events(mob, EntityEventType::TamingSucceeded) == 1, "the success did not show hearts");
            require(mob.getFlags().get(ActorFlag::Tamed), "tamed flag is not set");
        }

        MobActor *babyOf(const MobActor &parent) {
            for (auto &entry: mOwner.getActors()) {
                auto *mob = dynamic_cast<MobActor *>(entry.second.get());
                if (mob != nullptr && mob->getParentRuntimeId() == parent.getRuntimeId())
                    return mob;
            }
            return nullptr;
        }
    };

    void wolfTaming() {
        Arena arena;
        ServerPlayer &player = arena.player(7001);
        MobActor &wolf = arena.spawn("minecraft:wolf", "minecraft:spawn_wild_adult");
        require(!wolf.isTamed() && wolf.getComponent("minecraft:tameable") != nullptr, "wild wolf is not tameable");

        arena.tame(player, wolf, "minecraft:bone");
        require(wolf.hasComponentGroup("minecraft:wolf_tame") && !wolf.hasComponentGroup("minecraft:wolf_wild"),
                "on_tame did not swap the wild group for the tame one");
        require(wolf.getColor() == RED, "a new tame wolf does not wear a red collar");

        // a tamed wolf no longer takes bones as a tame item: the click toggles sitting
        arena.hold(player, nullptr, 0);
        arena.interact(player, wolf);
        require(wolf.isSitting() && wolf.getFlags().get(ActorFlag::Sitting), "owner click did not sit the wolf");
        arena.interact(player, wolf);
        require(!wolf.isSitting() && !wolf.getFlags().get(ActorFlag::Sitting), "owner click did not stand the wolf");

        ServerPlayer &stranger = arena.player(7002);
        arena.hold(stranger, nullptr, 0);
        arena.interact(stranger, wolf);
        require(!wolf.isSitting(), "a stranger sat someone else's wolf");
    }

    void wolfCollar() {
        Arena arena;
        ServerPlayer &player = arena.player(7101);
        MobActor &wolf = arena.spawn("minecraft:wolf", "minecraft:spawn_wild_adult");
        arena.hold(player, "minecraft:blue_dye", 2);
        arena.interact(player, wolf);
        require(wolf.getColor() == 0 && arena.held(player) == 2, "a wild wolf took a dye");

        arena.tame(player, wolf, "minecraft:bone");
        arena.hold(player, "minecraft:blue_dye", 2);
        arena.interact(player, wolf);
        require(wolf.getColor() == BLUE && arena.sentColor(wolf) == BLUE, "the dye did not color the collar");
        require(arena.held(player) == 1 && !wolf.isSitting(), "the dye was not used up alone");

        arena.interact(player, wolf);
        require(arena.held(player) == 1, "a dye of the collar's own color was used up");

        player.setGameType((int32_t) GameType::Creative);
        arena.hold(player, "minecraft:lime_dye", 1);
        arena.interact(player, wolf);
        require(wolf.getColor() == LIME && arena.held(player) == 1, "creative dyeing did not keep the dye");
    }

    void wolfHealing() {
        Arena arena;
        ServerPlayer &player = arena.player(7201);
        MobActor &wolf = arena.spawn("minecraft:wolf", "minecraft:spawn_wild_adult");
        arena.tame(player, wolf, "minecraft:bone");

        const float max = wolf.getMaxHealth();
        wolf.setHealth(max - 7.0f);
        arena.hold(player, "minecraft:beef", 2);
        arena.interact(player, wolf);
        require(wolf.getHealth() == max - 1.0f && arena.held(player) == 1, "raw beef did not heal 6");
        require(!wolf.isInLove(), "a hurt wolf went into love instead of eating");

        arena.interact(player, wolf);
        require(wolf.getHealth() == max && arena.held(player) == 0, "the heal did not stop at the maximum");

        arena.hold(player, "minecraft:porkchop", 1);
        arena.interact(player, wolf);
        require(wolf.isInLove() && arena.held(player) == 0, "a full health wolf did not take the food for love");

        arena.hold(player, "minecraft:bone", 1);
        wolf.setHealth(max - 1.0f);
        arena.interact(player, wolf);
        require(wolf.getHealth() == max - 1.0f && arena.held(player) == 1, "a bone healed the wolf");
    }

    void wolfBreeding() {
        Arena arena;
        ServerPlayer &player = arena.player(7301);
        MobActor &mother = arena.spawn("minecraft:wolf", "minecraft:spawn_wild_adult");
        MobActor &father = arena.spawn("minecraft:wolf", "minecraft:spawn_wild_adult");
        arena.tame(player, mother, "minecraft:bone");
        arena.tame(player, father, "minecraft:bone");
        arena.hold(player, "minecraft:yellow_dye", 1);
        arena.interact(player, father);
        require(mother.getColor() == RED && father.getColor() == YELLOW, "collars were not set up");

        for (MobActor *wolf: {&mother, &father}) {
            arena.hold(player, "minecraft:cooked_beef", 1);
            arena.interact(player, *wolf);
            require(wolf->isInLove(), "a full health tame wolf did not go into love");
        }

        BreedGoal sittingGoal(1.0f);
        arena.hold(player, nullptr, 0);
        arena.interact(player, mother);
        require(mother.isSitting() && !sittingGoal.canUse(arena.mOwner, mother), "a sitting wolf looked for a mate");
        arena.interact(player, mother);

        BreedGoal goal(1.0f);
        require(goal.canUse(arena.mOwner, mother), "the wolves did not find each other");
        goal.start(arena.mOwner, mother);
        for (int tick = 0; tick < 80 && arena.babyOf(mother) == nullptr; ++tick)
            goal.tick(arena.mOwner, mother);

        MobActor *puppy = arena.babyOf(mother);
        require(puppy != nullptr, "the wolves had no puppy");
        require(puppy->isOwnedBy(player) && puppy->getFlags().get(ActorFlag::Tamed),
                "the puppy of tamed wolves does not belong to their owner");
        require(puppy->hasComponentGroup("minecraft:wolf_tame") && puppy->hasComponentGroup("minecraft:wolf_baby"),
                "the puppy was not born tame");
        require(puppy->getColor() == ORANGE, "red and yellow collars did not mix into orange");
    }

    void wolfDefendsOwner() {
        Arena arena;
        ServerPlayer &player = arena.player(7351);
        MobActor &wolf = arena.spawn("minecraft:wolf", "minecraft:spawn_wild_adult");
        arena.tame(player, wolf, "minecraft:bone");
        MobActor &sheep = arena.spawn("minecraft:sheep", "spawn_adult");
        sheep.setPosition(Vector3f(2.5f, 100.0f, 0.5f));
        arena.hold(player, nullptr, 0);
        // the goal skips the tick it last handled, 0 at first: the hit is put on the next tick
        player.recordAttacked(sheep.getRuntimeId(), arena.mOwner.getCurrentTick() + 1);

        OwnerTargetGoal goal(OwnerTargetGoal::Mode::OwnerHurt);
        arena.interact(player, wolf);
        require(wolf.isSitting() && !goal.canUse(arena.mOwner, wolf), "a sitting wolf joined its owner's fight");
        arena.interact(player, wolf);
        require(goal.canUse(arena.mOwner, wolf), "the wolf ignored what its owner hit");
        goal.start(arena.mOwner, wolf);
        require(wolf.getTarget(arena.mOwner) == &sheep, "the wolf did not target what its owner hit");
    }

    void wolfPersistence() {
        Arena arena;
        ServerPlayer &player = arena.player(7401);
        MobActor &wolf = arena.spawn("minecraft:wolf", "minecraft:spawn_wild_adult");
        arena.tame(player, wolf, "minecraft:bone");
        arena.hold(player, "minecraft:blue_dye", 1);
        arena.interact(player, wolf);
        arena.hold(player, nullptr, 0);
        arena.interact(player, wolf);

        const Tag saved = wolf.saveNbt();
        require(saved.getByte("Color", 0) == BLUE && saved.getByte("Sitting", 0) == 1
                && saved.getLong("OwnerNew", -1) == player.getUniqueId(), "the save lost the wolf's state");

        auto *loaded = dynamic_cast<MobActor *>(arena.mOwner.spawnActor(arena.mLevel, "minecraft:wolf",
                Vector3f(0.5f, 100.0f, 0.5f), [&saved](ServerActor &actor) { actor.loadNbt(saved); }));
        require(loaded != nullptr, "the saved wolf did not load");
        loaded->startDefinition(arena.mOwner);
        require(loaded->isOwnedBy(player) && loaded->isSitting() && loaded->getColor() == BLUE,
                "the loaded wolf lost its owner, sitting or collar");
        require(loaded->hasComponentGroup("minecraft:on_tame_collar_color") && loaded->getColor() == BLUE,
                "the collar group put the red collar back on load");
    }

    void catTaming() {
        Arena arena;
        ServerPlayer &player = arena.player(7501);
        MobActor &cat = arena.spawn("minecraft:cat", "minecraft:spawn_wild_adult");
        arena.hold(player, "minecraft:bone", 1);
        arena.interact(player, cat);
        require(!cat.isTamed() && arena.held(player) == 1, "a bone tamed a cat");

        arena.tame(player, cat, "minecraft:cod");
        require(cat.hasComponentGroup("minecraft:cat_tame") && cat.getColor() == RED,
                "a new tame cat does not wear a red collar");

        arena.hold(player, "minecraft:lime_dye", 1);
        arena.interact(player, cat);
        require(cat.getColor() == LIME && arena.held(player) == 0, "the dye did not color the cat's collar");

        const float max = cat.getMaxHealth();
        cat.setHealth(max - 3.0f);
        arena.hold(player, "minecraft:salmon", 1);
        arena.interact(player, cat);
        require(cat.getHealth() == max - 1.0f && arena.held(player) == 0, "raw salmon did not heal the cat 2");

        arena.hold(player, nullptr, 0);
        arena.interact(player, cat);
        require(cat.isSitting(), "owner click did not sit the cat");

        MobActor &other = arena.spawn("minecraft:cat", "minecraft:spawn_wild_adult");
        arena.tame(player, other, "minecraft:salmon");
        BreedGoal goal(1.0f);
        cat.setHealth(max);
        for (MobActor *mob: {&cat, &other}) {
            arena.hold(player, "minecraft:cod", 1);
            arena.interact(player, *mob);
            require(mob->isInLove(), "a tame cat did not go into love");
        }
        require(goal.canUse(arena.mOwner, cat), "a sitting cat did not breed: its breedable allows sitting");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-taming-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        wolfTaming();
        wolfCollar();
        wolfHealing();
        wolfBreeding();
        wolfDefendsOwner();
        wolfPersistence();
        catTaming();
        std::cout << "taming checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
