#include "Core/Color/DyeColor.h"
#include "Network/Handler/InventoryHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/InventoryTransactionPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/BreedGoal.h"
#include "World/Actor/AI/Goal/EatBlockGoal.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/Storage/BlockStateUpgrades.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    const int32_t WHITE = 0;
    const int32_t ORANGE = 1;
    const int32_t YELLOW = 4;
    const int32_t GRAY = 7;
    const int32_t BLUE = 11;
    const int32_t RED = 14;
    const int32_t BLACK = 15;
    const char *const SHEAR_SOUND = "shear";
    const int32_t FACING_EAST = 5;
    const Vector3i DISPENSER(0, 100, 4);

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Sheep", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getOverworld();
        std::vector<LevelSoundEventPacket> mSounds;

        Arena() {
            require(mLevel.openStorage(ServerPaths::getRoot().string()), "could not open sheep storage");
            for (int x = -1; x <= 1; ++x) {
                for (int z = -1; z <= 1; ++z)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            mOwner.getNetworkHandler().setOutboundFilter([this](const NetworkIdentifier &, std::string &bytes) {
                ReadOnlyBinaryStream header(bytes);
                unsigned char sender = 0, recipient = 0;
                if (Packet::peekId(header, sender, recipient) != MinecraftPacketIds::LevelSoundEvent)
                    return false;
                ReadOnlyBinaryStream body(bytes);
                LevelSoundEventPacket packet;
                packet.readHeader(body);
                packet.read(body, mOwner.getCodecContext());
                mSounds.push_back(std::move(packet));
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
            // sounds only reach the viewers of the chunk
            for (int x = -1; x <= 1; ++x) {
                for (int z = -1; z <= 1; ++z)
                    player.getSentChunks().insert(((int64_t) x << 32) | (uint32_t) z);
            }
            player.getInventoryManager().attach(&player, &mOwner);
            return player;
        }

        MobActor &spawn(const char *event, const Vector3f &position = Vector3f(0.5f, 100.0f, 0.5f)) {
            auto *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, "minecraft:sheep", position,
                    [event](ServerActor &actor) {
                        static_cast<MobActor &>(actor).setSpawnEvent(event);
                    }));
            require(mob != nullptr, "sheep did not spawn");
            mob->startDefinition(mOwner);
            return *mob;
        }

        void hold(ServerPlayer &player, const char *item, int count) {
            player.getInventory().setSelectedSlot(0);
            player.getInventory().setItem(0, item == nullptr ? ItemStack::air() : mOwner.createItemStack(item, count));
        }

        const ItemStack &held(ServerPlayer &player) {
            return player.getInventory().getItemInHand();
        }

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

        // wool items on the ground, by identifier, removed as they are counted
        int32_t collect(const std::string &identifier) {
            int32_t count = 0;
            for (auto &entity: mOwner.getItemEntities()) {
                ItemActor *item = entity.get();
                if (item == nullptr || item->getItem().isAir())
                    continue;
                if (item->getItem().mDefinition->getIdentifier() == identifier) {
                    count += item->getItem().mCount;
                    item->setItem(ItemStack::air());
                }
            }
            return count;
        }

        int32_t sounds(const std::string &sound) const {
            int32_t count = 0;
            for (const LevelSoundEventPacket &packet: mSounds) {
                if (packet.mSound == sound)
                    ++count;
            }
            return count;
        }

        MobActor *babyOf(const MobActor &parent) {
            for (auto &entry: mOwner.getActors()) {
                auto *mob = dynamic_cast<MobActor *>(entry.second.get());
                if (mob != nullptr && mob->getParentRuntimeId() == parent.getRuntimeId())
                    return mob;
            }
            return nullptr;
        }

        int32_t breed(ServerPlayer &player, MobActor &mother, MobActor &father) {
            for (MobActor *sheep: {&mother, &father}) {
                hold(player, "minecraft:wheat", 1);
                interact(player, *sheep);
                require(sheep->isInLove(), "wheat did not put a sheep in love");
            }
            BreedGoal goal(1.0f);
            require(goal.canUse(mOwner, mother), "the sheep did not find each other");
            goal.start(mOwner, mother);
            for (int tick = 0; tick < 80 && babyOf(mother) == nullptr; ++tick)
                goal.tick(mOwner, mother);
            MobActor *lamb = babyOf(mother);
            require(lamb != nullptr && lamb->hasComponentGroup("minecraft:sheep_baby"), "the sheep had no lamb");
            return lamb->getColor();
        }
    };

    bool sheared(const MobActor &sheep) {
        return sheep.getComponent("minecraft:is_sheared") != nullptr;
    }

    void shearing() {
        Arena arena;
        ServerPlayer &player = arena.player(8001);
        MobActor &sheep = arena.spawn("spawn_adult");
        const int32_t color = sheep.getColor();
        const std::string wool = DyeColor::identifier((uint8_t) color, "_wool");
        arena.hold(player, "minecraft:shears", 1);

        arena.interact(player, sheep);
        require(sheared(sheep) && sheep.getComponent("minecraft:is_dyeable") == nullptr, "the shears did not shear");
        const int32_t dropped = arena.collect(wool);
        require(dropped >= 1 && dropped <= 3, "shearing did not drop one to three wool of the sheep's color");
        require(arena.held(player).mDamage == 1, "shearing did not wear the shears");
        require(arena.sounds(SHEAR_SOUND) == 1, "shearing made no shear sound");
        sheep.tick(arena.mOwner);
        require(sheep.getFlags().get(ActorFlag::Sheared), "viewers were not told the sheep is sheared");

        arena.interact(player, sheep);
        require(arena.collect(wool) == 0 && arena.held(player).mDamage == 1, "a sheared sheep was sheared again");

        MobActor &lamb = arena.spawn("spawn_baby");
        arena.interact(player, lamb);
        require(!sheared(lamb) && arena.held(player).mDamage == 1, "a lamb was sheared");

        player.setGameType((int32_t) GameType::Creative);
        MobActor &other = arena.spawn("spawn_adult");
        arena.interact(player, other);
        require(sheared(other) && arena.held(player).mDamage == 1, "creative shearing wore the shears");
    }

    void dyeing() {
        Arena arena;
        ServerPlayer &player = arena.player(8101);
        MobActor &sheep = arena.spawn("spawn_adult");
        sheep.fireEvent(arena.mOwner, "wololo");
        require(sheep.getColor() == RED, "wololo did not turn the sheep red");

        arena.hold(player, "minecraft:blue_dye", 2);
        arena.interact(player, sheep);
        require(sheep.getColor() == BLUE && arena.held(player).mCount == 1, "the dye did not color the wool");
        arena.interact(player, sheep);
        require(arena.held(player).mCount == 1, "a dye of the sheep's own color was used up");

        arena.hold(player, "minecraft:shears", 1);
        arena.interact(player, sheep);
        require(arena.collect("minecraft:blue_wool") >= 1, "a dyed sheep did not drop wool of its dye");

        arena.hold(player, "minecraft:lime_dye", 1);
        arena.interact(player, sheep);
        require(sheep.getColor() == BLUE && arena.held(player).mCount == 1, "a sheared sheep took a dye");
    }

    void regrowing() {
        Arena arena;
        MobActor &sheep = arena.spawn("spawn_adult");
        sheep.fireEvent(arena.mOwner, "minecraft:on_sheared");
        const int32_t color = sheep.getColor();
        const Vector3i below(0, 99, 0);
        arena.mLevel.setBlock(below, VanillaBlocks::fromIdentifier("minecraft:grass_block")->toBlockState(), false);

        // the definition's pair and event, with a certain roll and the 1.8 s it takes to eat
        const json::Value *eat = sheep.getComponent("minecraft:behavior.eat_block");
        require(eat != nullptr, "the sheep has no eat_block behavior");
        EatBlockGoal goal({{BlockStateUpgrades::currentName("grass"), BlockStateUpgrades::currentName("dirt")}}, "1",
                          36, "minecraft:on_eat_block");
        require(goal.canUse(arena.mOwner, sheep), "the sheep did not find the grass under it");
        goal.start(arena.mOwner, sheep);
        for (int tick = 0; tick < 36; ++tick)
            goal.tick(arena.mOwner, sheep);
        require(!sheared(sheep) && sheep.getComponent("minecraft:is_dyeable") != nullptr,
                "eating grass did not grow the wool back");
        require(arena.mLevel.getBlockState(below.x, below.y, below.z).mName == "minecraft:dirt",
                "the eaten grass did not turn to dirt");
        require(sheep.getColor() == color, "the wool grew back in another color");
    }

    void breeding() {
        Arena arena;
        ServerPlayer &player = arena.player(8201);
        MobActor &red = arena.spawn("spawn_adult");
        MobActor &yellow = arena.spawn("spawn_adult");
        red.fireEvent(arena.mOwner, "wololo");
        arena.hold(player, "minecraft:yellow_dye", 1);
        arena.interact(player, yellow);
        require(arena.breed(player, red, yellow) == ORANGE, "red and yellow sheep did not have an orange lamb");

        MobActor &white = arena.spawn("spawn_adult");
        MobActor &black = arena.spawn("spawn_adult");
        arena.hold(player, "minecraft:white_dye", 1);
        arena.interact(player, white);
        arena.hold(player, "minecraft:black_dye", 1);
        arena.interact(player, black);
        require(white.getColor() == WHITE && black.getColor() == BLACK, "the parents were not dyed");
        require(arena.breed(player, white, black) == GRAY, "white and black sheep did not have a gray lamb");

        MobActor &first = arena.spawn("spawn_adult");
        MobActor &second = arena.spawn("spawn_adult");
        for (MobActor *sheep: {&first, &second}) {
            arena.hold(player, "minecraft:blue_dye", 1);
            arena.interact(player, *sheep);
        }
        require(arena.breed(player, first, second) == BLUE, "two blue sheep did not have a blue lamb");

        MobActor &blue = arena.spawn("spawn_adult");
        MobActor &yellowAgain = arena.spawn("spawn_adult");
        arena.hold(player, "minecraft:blue_dye", 1);
        arena.interact(player, blue);
        arena.hold(player, "minecraft:yellow_dye", 1);
        arena.interact(player, yellowAgain);
        const int32_t unmixed = arena.breed(player, blue, yellowAgain);
        require(unmixed == BLUE || unmixed == YELLOW, "colors with no dye recipe did not pass on a parent's");
    }

    void persistence() {
        Arena arena;
        ServerPlayer &player = arena.player(8301);
        MobActor &sheep = arena.spawn("spawn_adult");
        arena.hold(player, "minecraft:blue_dye", 1);
        arena.interact(player, sheep);
        arena.hold(player, "minecraft:shears", 1);
        arena.interact(player, sheep);

        const Tag saved = sheep.saveNbt();
        require(saved.getByte("Color", 0) == BLUE && saved.getByte("Sheared", 0) == 1, "the save lost wool state");
        auto *loaded = dynamic_cast<MobActor *>(arena.mOwner.spawnActor(arena.mLevel, "minecraft:sheep",
                Vector3f(0.5f, 100.0f, 0.5f), [&saved](ServerActor &actor) { actor.loadNbt(saved); }));
        require(loaded != nullptr, "the saved sheep did not load");
        loaded->startDefinition(arena.mOwner);
        require(loaded->getColor() == BLUE && sheared(*loaded), "the loaded sheep lost its color or shearing");

        loaded->fireEvent(arena.mOwner, "minecraft:on_eat_block");
        require(!sheared(*loaded) && loaded->getColor() == BLUE, "regrown wool lost the saved dye");
    }

    struct Dispenser {
        Arena &mArena;
        DispenserBlockActor *mActor = nullptr;
        BlockState mState;

        explicit Dispenser(Arena &arena) : mArena(arena) {
            mState = VanillaBlocks::fromIdentifier("minecraft:dispenser")->toBlockState();
            Tag states = mState.mStates;
            states.putInt("facing_direction", FACING_EAST);
            mState = BlockState(mState.mName, states);
            arena.mLevel.setBlock(DISPENSER, mState, false);
            mActor = &arena.mLevel.getBlockActors().getOrCreate<DispenserBlockActor>(DISPENSER);
            mActor->getInventory().setContainerItem(0, arena.mOwner.createItemStack("minecraft:shears", 1));
        }

        void fire() {
            VanillaBlocks::fromIdentifier("minecraft:dispenser")->onScheduledUpdate(mArena.mOwner, mArena.mLevel,
                                                                                    DISPENSER, mState);
        }

        const ItemStack &shears() const {
            return mActor->getInventory().getContainerItem(0);
        }
    };

    void dispensing() {
        Arena arena;
        arena.player(8401);
        Dispenser dispenser(arena);
        const Vector3f front((float) DISPENSER.x + 1.5f, (float) DISPENSER.y, (float) DISPENSER.z + 0.5f);
        MobActor &lamb = arena.spawn("spawn_baby", front);
        MobActor &sheep = arena.spawn("spawn_adult", front);
        const std::string wool = DyeColor::identifier((uint8_t) sheep.getColor(), "_wool");

        dispenser.fire();
        require(sheared(sheep) && !sheared(lamb), "the dispenser did not shear the adult sheep alone");
        const int32_t dropped = arena.collect(wool);
        require(dropped >= 1 && dropped <= 3, "the dispenser's shearing did not drop the sheep's wool");
        require(!dispenser.shears().isAir() && dispenser.shears().mDamage == 1,
                "the dispenser did not wear its shears");
        require(arena.sounds(SHEAR_SOUND) == 1, "the dispenser's shearing made no shear sound");

        dispenser.fire();
        require(arena.collect(wool) == 0, "the dispenser sheared a sheared sheep");
        require(dispenser.shears().isAir() || dispenser.shears().mDamage == 1,
                "the dispenser wore its shears without shearing");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-sheep-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        shearing();
        dyeing();
        regrowing();
        breeding();
        persistence();
        dispensing();
        std::cout << "sheep checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
