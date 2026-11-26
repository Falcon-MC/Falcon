#include "Falcon/Plugin/PluginRegistrationScope.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Mob/Hostile/AbstractSlimeActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Projectile/ProjectileActor.h"
#include "World/Actor/ServerActor.h"
#include "World/Item/ItemClassRegistry.h"
#include "World/Item/VanillaItems.h"
#include "World/Level/Block/Actor/BlockActorClassRegistry.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/MobSpawnerBlockActor.h"
#include "World/Level/Block/Actor/VaultBlockActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Actor/TrialSpawnerBlockActor.h"
#include "World/Level/Storage/Loot/LootTableRegistry.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    const char *const BLAZE = "minecraft:blaze";

    struct Arena {
        ServerNetworkHandler mOwner{"Mob spawner", "", 1};
        Level *mLevel = nullptr;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "normal.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -4; x <= 4; x++) {
                for (int32_t z = -4; z <= 4; z++) {
                    // the opened world has generated the chunk its random seed spawns in, and an existing chunk wins
                    // the insert
                    mLevel->dropChunk(x, z);
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
                }
            }
        }

        ServerPlayer &player(uint64_t runtimeId, const Vector3f &position) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(*mLevel);
            player.setPosition(position);
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            return player;
        }

        // a stone floor under an open room around the spawner
        MobSpawnerBlockActor &spawner(const Vector3i &position, const char *identifier) {
            for (int32_t x = position.x - 10; x <= position.x + 10; ++x) {
                for (int32_t z = position.z - 10; z <= position.z + 10; ++z) {
                    mLevel->setBlockState(x, position.y - 2, z, BlockState("minecraft:stone"));
                    for (int32_t y = position.y - 1; y <= position.y + 3; ++y)
                        mLevel->setBlockState(x, y, z, BlockState("minecraft:air"));
                }
            }
            mLevel->setBlockState(position.x, position.y, position.z, BlockState("minecraft:mob_spawner"));
            MobSpawnerBlockActor &spawner = mLevel->getBlockActors().getOrCreate<MobSpawnerBlockActor>(position);
            if (identifier != nullptr)
                spawner.setEntityIdentifier(identifier);
            spawner.seedRandom(1234);
            return spawner;
        }

        std::vector<ServerActor *> all(const char *identifier) {
            std::vector<ServerActor *> found;
            for (auto &entry: mOwner.getActors()) {
                if (entry.second->getTypeId() == identifier && !entry.second->isDead() && entry.second->isIn(*mLevel))
                    found.push_back(entry.second.get());
            }
            return found;
        }

        void clear() {
            std::vector<int64_t> ids;
            for (auto &entry: mOwner.getActors())
                ids.push_back(entry.first);
            for (const int64_t id: ids)
                mOwner.removeActor(id);
        }
    };

    void defaultsAndSave(Arena &arena) {
        MobSpawnerBlockActor spawner;
        require(spawner.getDelay() == 20 && spawner.getMinSpawnDelay() == 200 && spawner.getMaxSpawnDelay() == 800,
                "the spawner delays are not the game's defaults");
        require(spawner.getSpawnCount() == 4 && spawner.getMaxNearbyEntities() == 6
                        && spawner.getRequiredPlayerRange() == 16 && spawner.getSpawnRange() == 4,
                "the spawner counts are not the game's defaults");
        require(!spawner.hasEntity(), "an empty spawner has a mob");

        spawner.setEntityIdentifier(BLAZE);
        spawner.setDelay(37);
        const Tag saved = spawner.saveNbt();
        require(saved.getString("EntityIdentifier") == BLAZE, "the mob was not saved");
        require(saved.contains("Delay", Tag::Type::Short) && saved.getShort("Delay") == 37, "the delay is not a short");
        require(saved.contains("DisplayEntityHeight", Tag::Type::Float), "the display height is not saved");

        Tag changed = saved;
        changed.putShort("MinSpawnDelay", 10);
        changed.putShort("MaxSpawnDelay", 20);
        changed.putShort("SpawnCount", 2);
        changed.putShort("SpawnRange", 3);
        Tag potentials = Tag::ofList(Tag::Type::Compound);
        Tag entry = Tag::ofCompound();
        entry.putInt("Weight", 1);
        entry.putString("TypeId", "minecraft:zombie");
        potentials.addToList(entry);
        changed.put("SpawnPotentials", potentials);

        MobSpawnerBlockActor loaded;
        loaded.loadNbt(changed, arena.mOwner.getCodecContext());
        require(loaded.hasEntity() && loaded.getEntityIdentifier() == BLAZE && loaded.getDelay() == 37,
                "the saved spawner did not load back");
        require(loaded.getMinSpawnDelay() == 10 && loaded.getMaxSpawnDelay() == 20 && loaded.getSpawnCount() == 2
                        && loaded.getSpawnRange() == 3,
                "the delays and counts were not read");
        require(loaded.getSpawnPotentials().size() == 1
                        && loaded.getSpawnPotentials()[0].mIdentifier == "minecraft:zombie",
                "the spawn potentials were not read");

        // the delay pair is only read as shorts
        Tag ints = saved;
        ints.putInt("MinSpawnDelay", 5);
        MobSpawnerBlockActor ignored;
        ignored.loadNbt(ints, arena.mOwner.getCodecContext());
        require(ignored.getMinSpawnDelay() == 200 && ignored.getMaxSpawnDelay() == 800,
                "delays saved as ints were read");
    }

    void spawnsNearPlayer(Arena &arena) {
        arena.clear();
        const Vector3i position(0, 80, 0);
        MobSpawnerBlockActor &spawner = arena.spawner(position, BLAZE);

        // nobody around: the delay does not run down
        BlockActorStore &store = arena.mLevel->getBlockActors();
        store.tick(arena.mOwner);
        require(spawner.getDelay() == 20, "the spawner counted down with no player in range");

        ServerPlayer &player = arena.player(9501, Vector3f(30.5f, 79.0f, 0.5f));
        store.tick(arena.mOwner);
        require(spawner.getDelay() == 20, "a player 30 blocks away activated the spawner");

        player.setPosition(Vector3f(8.5f, 79.0f, 0.5f));
        store.tick(arena.mOwner);
        require(spawner.getDelay() == 19, "a player 8 blocks away did not activate the spawner");

        spawner.setDelay(0);
        int32_t turns = 0;
        while (arena.all(BLAZE).empty() && turns++ < 50)
            store.tick(arena.mOwner);
        const std::vector<ServerActor *> blazes = arena.all(BLAZE);
        require(!blazes.empty() && blazes.size() <= 4, "the spawner did not spawn one to four blazes");
        require(spawner.getDelay() >= 200 && spawner.getDelay() < 800, "the delay was not drawn between 200 and 800");

        for (const ServerActor *blaze: blazes) {
            const Vector3f &at = blaze->getPosition();
            const int32_t dx = (int32_t) std::floor(at.x - (float) position.x);
            const int32_t dy = (int32_t) std::floor(at.y - (float) position.y);
            const int32_t dz = (int32_t) std::floor(at.z - (float) position.z);
            require(std::abs(dx) + std::abs(dy) + std::abs(dz) <= 4, "a blaze spawned out of the spawn range");
            require(dy >= -1 && dy <= 1, "a blaze spawned more than a block above or below the spawner");
        }
        arena.mOwner.erasePlayer(player.getNetworkIdentifier());
    }

    void stopsAtCap(Arena &arena) {
        arena.clear();
        const Vector3i position(32, 80, 32);
        MobSpawnerBlockActor &spawner = arena.spawner(position, BLAZE);
        ServerPlayer &player = arena.player(9502, Vector3f(36.5f, 79.0f, 32.5f));

        // six blazes inside the counted box: the next turn only draws a new delay
        for (int32_t i = 0; i < 6; ++i)
            arena.mOwner.spawnActor(*arena.mLevel, BLAZE, Vector3f(25.5f + (float) i, 79.0f, 28.5f));
        require(spawner.countNearby(arena.mOwner, *arena.mLevel, BLAZE) == 6, "the six blazes were not counted");

        spawner.setDelay(0);
        arena.mLevel->getBlockActors().tick(arena.mOwner);
        require(arena.all(BLAZE).size() == 6, "the spawner spawned past its cap of six");
        require(spawner.getDelay() >= 200, "a capped spawner did not take a new delay");

        // a blaze nine blocks out is outside the box and not counted
        arena.clear();
        arena.mOwner.spawnActor(*arena.mLevel, BLAZE, Vector3f(41.5f, 79.0f, 32.5f));
        require(spawner.countNearby(arena.mOwner, *arena.mLevel, BLAZE) == 0, "a blaze outside the box was counted");
        arena.mOwner.erasePlayer(player.getNetworkIdentifier());
    }

    void needsSpace(Arena &arena) {
        arena.clear();
        const Vector3i position(-32, 80, -32);
        MobSpawnerBlockActor &spawner = arena.spawner(position, BLAZE);
        ServerPlayer &player = arena.player(9503, Vector3f(-28.5f, 79.0f, -32.5f));

        // every spot it can draw is stone: the turn spawns nothing and tries again
        for (int32_t x = position.x - 5; x <= position.x + 5; ++x) {
            for (int32_t z = position.z - 5; z <= position.z + 5; ++z) {
                for (int32_t y = position.y - 1; y <= position.y + 1; ++y) {
                    if (x != position.x || y != position.y || z != position.z)
                        arena.mLevel->setBlockState(x, y, z, BlockState("minecraft:stone"));
                }
            }
        }
        spawner.setDelay(0);
        for (int32_t turn = 0; turn < 5; ++turn)
            arena.mLevel->getBlockActors().tick(arena.mOwner);
        require(arena.all(BLAZE).empty(), "a blaze spawned inside stone");
        require(spawner.getDelay() == 0, "a turn that spawned nothing took a new delay");
        arena.mOwner.erasePlayer(player.getNetworkIdentifier());
    }

    void emptySpawner(Arena &arena) {
        arena.clear();
        MobSpawnerBlockActor &spawner = arena.spawner(Vector3i(0, 80, 40), nullptr);
        ServerPlayer &player = arena.player(9504, Vector3f(2.5f, 79.0f, 40.5f));
        spawner.setDelay(0);
        arena.mLevel->getBlockActors().tick(arena.mOwner);
        require(arena.mOwner.getActors().empty() && spawner.getDelay() == 0, "an empty spawner did something");
        arena.mOwner.erasePlayer(player.getNetworkIdentifier());
    }

    class PluginVault final : public BlockActor {
    public:
        const char *getBlockActorId() const override {
            return VaultBlockActor::BLOCK_ACTOR_ID;
        }

        Tag saveNbt() const override {
            return mData;
        }

        void loadNbt(const Tag &data, const PacketCodecContext &) override {
            mData = data;
        }

    private:
        Tag mData;
    };

    void generatedVaultUsesPluginFactory(Arena &arena) {
        const Vector3i pos(33, 80, 32);
        arena.mLevel->setBlockState(pos.x, pos.y, pos.z, BlockState("minecraft:vault"));
        Tag data = Tag::ofCompound();
        data.putString("id", VaultBlockActor::BLOCK_ACTOR_ID);
        data.putInt("x", pos.x);
        data.putInt("y", pos.y);
        data.putInt("z", pos.z);
        data.putInt("plugin_data", 42);

        int pluginOwner = 0;
        {
            const PluginRegistrationScope scope(&pluginOwner, true);
            BlockActorClassRegistry::Registration(VaultBlockActor::BLOCK_ACTOR_ID,
                                                  []() -> std::unique_ptr<BlockActor> {
                                                      return std::make_unique<PluginVault>();
                                                  });
        }
        auto &actors = arena.mLevel->getBlockActors();
        actors.keepGenerated(data);
        BlockActorClassRegistry::remove(&pluginOwner);

        auto *vault = actors.find<PluginVault>(pos);
        require(vault != nullptr, "generated vault ignored the plugin factory");
        require(vault->saveNbt().getInt("plugin_data") == 42, "generated vault did not load the plugin data");
        actors.remove(pos);
    }

    void vaultCycle(Arena &arena, bool ominous) {
        arena.mOwner.getItemEntities().clear();
        const Vector3i pos(32, 80, 32);
        Tag states = Tag::ofCompound();
        states.putString("vault_state", "inactive");
        states.putByte("ominous", ominous ? 1 : 0);
        const BlockState placed("minecraft:vault", states);
        arena.mLevel->setBlockState(pos.x, pos.y, pos.z, placed);

        Tag data = Tag::ofCompound();
        data.putString("id", "Vault");
        data.putInt("x", pos.x);
        data.putInt("y", pos.y);
        data.putInt("z", pos.z);
        auto &actors = arena.mLevel->getBlockActors();
        actors.keepGenerated(data);
        auto *vault = actors.find<VaultBlockActor>(pos);
        require(vault != nullptr, "generated vault did not get its block actor");

        ServerPlayer &player = arena.player(9600, Vector3f(34.5f, 80.0f, 32.5f));
        player.setUuid("vault-test-player");
        player.setUniqueId(9600);
        const auto *block = VanillaBlocks::fromState(placed);
        const char *correct = ominous ? "minecraft:ominous_trial_key" : "minecraft:trial_key";
        const char *incorrect = ominous ? "minecraft:trial_key" : "minecraft:ominous_trial_key";

        require(vault->tick(arena.mOwner), "vault removed itself");
        const Tag preview = vault->getSpawnCompound();
        require(preview.contains("display_item", Tag::Type::Compound), "vault did not display loot");
        const Tag *connected = preview.get("connected_players");
        require(connected != nullptr && connected->getList().size() == 1
                        && connected->getList().front().asLong() == 9600,
                "vault did not send the connected player");

        player.getInventory().setItemInHand(arena.mOwner.createItemStack(incorrect, 2));
        require(block != nullptr && block->onInteract(arena.mOwner, player, pos, placed), "vault interaction fell through");
        require(player.getInventory().getItemInHand().mCount == 2, "vault consumed the wrong key");
        player.getInventory().setItemInHand(arena.mOwner.createItemStack(correct, 2));
        require(block->onInteract(arena.mOwner, player, pos, placed), "vault did not handle the right key");
        require(player.getInventory().getItemInHand().mCount == 1, "vault did not consume the right key");

        const auto stored = actors.saveChunk(pos.x >> 4, pos.z >> 4);
        actors.remove(pos);
        actors.addLoaded(stored, arena.mOwner.getCodecContext());
        vault = actors.find<VaultBlockActor>(pos);
        require(vault != nullptr, "vault did not reload");
        for (int i = 0; i < 33; ++i)
            require(vault->tick(arena.mOwner), "vault stopped ticking");
        require(arena.mOwner.getItemEntities().empty(), "vault ejected before opening the shutter");
        require(vault->tick(arena.mOwner) && arena.mOwner.getItemEntities().size() == 1,
                "vault did not eject its first item after opening the shutter");
        for (int i = 0; i < 400; ++i)
            require(vault->tick(arena.mOwner), "vault stopped ticking");
        require(!arena.mOwner.getItemEntities().empty(), "vault lost its queued loot on reload");
        const size_t count = arena.mOwner.getItemEntities().size();
        ServerPlayer &visitor = arena.player(9601, Vector3f(34.5f, 80.0f, 32.5f));
        visitor.setUuid("vault-second-player");
        visitor.setUniqueId(9601);
        visitor.getInventory().setItemInHand(arena.mOwner.createItemStack(correct, 1));
        for (int i = 0; i < 20; ++i)
            require(vault->tick(arena.mOwner), "vault did not activate for the next player");

        player.setUniqueId(9610);
        require(!vault->unlock(arena.mOwner, player), "vault rewarded the same account twice");
        require(player.getInventory().getItemInHand().mCount == 1, "vault consumed a repeat player's key");
        require(arena.mOwner.getItemEntities().size() == count, "vault duplicated loot");
        arena.mOwner.erasePlayer(player.getNetworkIdentifier());
        require(vault->unlock(arena.mOwner, visitor), "vault did not accept a different player");
        require(visitor.getInventory().getItemInHand().isAir(), "vault did not consume the second player's key");
        arena.mOwner.erasePlayer(visitor.getNetworkIdentifier());
        actors.remove(pos);
    }

    void trialSpawnerData(Arena &arena) {
        auto actor = BlockActorStore::create(TrialSpawnerBlockActor::BLOCK_ACTOR_ID);
        require(dynamic_cast<TrialSpawnerBlockActor *>(actor.get()) != nullptr, "trial spawner actor is not registered");

        Tag data = Tag::ofCompound();
        Tag normal = Tag::ofCompound();
        normal.putInt("ticks_between_spawn", 20);
        data.put("normal_config", normal);
        data.putString("ominous_config", "minecraft:trial_chamber/melee/zombie/ominous");
        data = TrialSpawnerBlockActor::createTrialData(data);
        Tag mobs = Tag::ofList(Tag::Type::Compound);
        Tag mob = Tag::ofCompound();
        mob.putLong("uuid", 0x100000001LL);
        mobs.addToList(mob);
        data.put("current_mobs", mobs);
        data.putLong("cooldown_end_at", 36000);
        actor->loadNbt(data, arena.mOwner.getCodecContext());

        const Tag saved = actor->saveNbt();
        const Tag *config = saved.get("normal_config");
        require(config != nullptr && config->getInt("ticks_between_spawn") == 20, "trial config was lost");
        require(saved.getString("ominous_config") == data.getString("ominous_config"), "trial preset was lost");
        require(saved.getLong("cooldown_end_at") == 36000, "trial cooldown was lost");

        Tag tables = Tag::ofList(Tag::Type::Compound);
        Tag keyTable = Tag::ofCompound();
        keyTable.putString("data", "minecraft:spawners/ominous/trial_chamber/key");
        keyTable.putInt("weight", 3);
        tables.addToList(keyTable);
        Tag rewardConfig = Tag::ofCompound();
        rewardConfig.put("loot_tables_to_eject", tables);
        std::mt19937 random(42);
        Tag entity = Tag::ofCompound();
        entity.putString("id", "minecraft:zombie");
        entity.putByte("IsBaby", 1);
        Tag spawn = Tag::ofCompound();
        spawn.put("entity", entity);
        Tag potential = Tag::ofCompound();
        potential.put("data", spawn);
        potential.putInt("weight", 1);
        Tag potentials = Tag::ofList(Tag::Type::Compound);
        potentials.addToList(potential);
        normal.put("spawn_potentials", potentials);
        const Tag selected = TrialSpawnerBlockActor::rollSpawnData(normal, random);
        const Tag *selectedEntity = selected.get("entity");
        require(selectedEntity != nullptr && selectedEntity->getString("id") == "minecraft:zombie"
                        && selectedEntity->getByte("IsBaby") == 1,
                "trial spawn selection lost the entity configuration");

        const auto rewards = TrialSpawnerBlockActor::rollReward(rewardConfig, random);
        require(rewards.size() == 1 && rewards.front().mIdentifier == "minecraft:ominous_trial_key",
                "namespaced trial reward did not resolve to the ominous trial key");

        const Tag *tracked = saved.get("current_mobs");
        require(tracked != nullptr && tracked->isList() && tracked->getList().size() == 1
                        && tracked->getList().front().getLong("uuid") == 0x100000001LL,
                "tracked mob id was lost or truncated");

        const LootTableRegistry &loot = LootTableRegistry::getInstance();
        for (const char *path: {"spawners/trial_chamber/key.json", "spawners/trial_chamber/consumables.json",
                                "spawners/ominous/trial_chamber/key.json",
                                "spawners/ominous/trial_chamber/consumables.json",
                                "spawners/trial_chamber/items_to_drop_when_ominous.json"}) {
            require(loot.get(path) != nullptr, "trial spawner loot is missing from BedrockData");
        }
    }

    void trialSpawnerCycle(Arena &arena, bool ominous, int32_t cooldown, bool unloadMob = false) {
        arena.clear();
        arena.mOwner.getItemEntities().clear();
        const Vector3i position(40, 80, 40);
        arena.spawner(position, nullptr);
        Tag states = Tag::ofCompound();
        states.putByte("ominous", 0);
        states.putInt("trial_spawner_state", 1);
        arena.mLevel->setBlockState(position.x, position.y, position.z, BlockState("minecraft:trial_spawner", states));
        auto &spawner = arena.mLevel->getBlockActors().getOrCreate<TrialSpawnerBlockActor>(position);

        Tag entity = Tag::ofCompound();
        entity.putString("id", BLAZE);
        Tag spawnData = Tag::ofCompound();
        spawnData.put("entity", entity);
        Tag potential = Tag::ofCompound();
        potential.put("data", spawnData);
        potential.putInt("weight", 1);
        Tag potentials = Tag::ofList(Tag::Type::Compound);
        potentials.addToList(potential);
        Tag config = Tag::ofCompound();
        config.put("spawn_potentials", potentials);
        config.putFloat("total_mobs", 1.0f);
        config.putFloat("simultaneous_mobs", 1.0f);
        if (cooldown != 36000)
            config.putInt("target_cooldown_length", cooldown);
        Tag reward = Tag::ofCompound();
        reward.putString("data", ominous ? "minecraft:spawners/ominous/trial_chamber/key"
                                         : "minecraft:spawners/trial_chamber/key");
        reward.putInt("weight", 1);
        Tag rewards = Tag::ofList(Tag::Type::Compound);
        rewards.addToList(reward);
        config.put("loot_tables_to_eject", rewards);

        Tag trial = Tag::ofCompound();
        trial.put("normal_config", config);
        config.remove("spawn_potentials");
        trial.put("ominous_config", config);
        spawner.loadTrialData(TrialSpawnerBlockActor::createTrialData(trial));
        ServerPlayer &player = arena.player(9506, Vector3f(42.5f, 79.0f, 40.5f));
        player.setUniqueId(9506);
        if (ominous)
            arena.mOwner.addEffect(player, MobEffectId::BadOmen, 1, 100);

        const auto step = [&]() {
            arena.mLevel->tickTime();
            require(spawner.tick(arena.mOwner), "trial spawner removed itself");
        };
        const auto state = [&]() {
            return arena.mLevel->peekBlockPtr(position.x, position.y, position.z)->mStates;
        };
        if (ominous) {
            step();
            require(state().getByte("ominous") == 1 && !player.hasEffect(MobEffectId::BadOmen),
                    "bad omen did not convert the trial");
            const auto *omen = player.getEffect(MobEffectId::TrialOmen);
            require(omen != nullptr && omen->mDuration == 36000, "trial omen duration did not scale");
        }
        step();
        require(state().getInt("trial_spawner_state") == 2, "trial did not activate");
        if (!ominous) {
            for (int32_t x = position.x - 4; x <= position.x + 4; ++x) {
                for (int32_t z = position.z - 4; z <= position.z + 4; ++z) {
                    arena.mLevel->setBlockState(x, position.y + 1, z, BlockState("minecraft:stone"));
                    if (x != position.x || z != position.z)
                        arena.mLevel->setBlockState(x, position.y, z, BlockState("minecraft:stone"));
                }
            }
            const Tag before = spawner.getSpawnCompound();
            for (int i = 0; i < 20; ++i)
                step();
            const Tag after = spawner.getSpawnCompound();
            require(before.get("spawn_data") != nullptr && after.get("spawn_data") != nullptr,
                    "trial lost the selected mob while checking spawn space");
            require(*before.get("spawn_data") == *after.get("spawn_data"),
                    "trial rerolled the mob after a blocked spawn attempt");
            for (int32_t x = position.x - 4; x <= position.x + 4; ++x) {
                for (int32_t z = position.z - 4; z <= position.z + 4; ++z) {
                    arena.mLevel->setBlockState(x, position.y + 1, z, BlockState("minecraft:air"));
                    if (x != position.x || z != position.z)
                        arena.mLevel->setBlockState(x, position.y, z, BlockState("minecraft:air"));
                }
            }
        }
        for (int i = 0; i < 100 && arena.all(BLAZE).empty(); ++i)
            step();
        const auto mobs = arena.all(BLAZE);
        require(mobs.size() == 1 && mobs.front()->isPersistent(), "trial did not spawn a persistent mob");
        if (ominous && cooldown == 36000) {
            size_t projectiles = 0;
            for (int i = 0; i < 281 && projectiles == 0; ++i) {
                step();
                for (const auto &entry: arena.mOwner.getActors()) {
                    if (dynamic_cast<ProjectileActor *>(entry.second.get()) != nullptr)
                        ++projectiles;
                }
            }
            require(projectiles == 1, "active ominous trial did not release its attack projectile");
        }
        const int64_t mobId = mobs.front()->getUniqueId();
        if (unloadMob) {
            const Vector3f mobPosition = mobs.front()->getPosition();
            const int32_t chunkX = (int32_t) std::floor(mobPosition.x) >> 4;
            const int32_t chunkZ = (int32_t) std::floor(mobPosition.z) >> 4;
            arena.mOwner.saveActorsForChunk(*arena.mLevel, chunkX, chunkZ, true);
            require(arena.mOwner.getActor(mobId) == nullptr && arena.mLevel->hasStoredEntity(mobId),
                    "trial mob was not unloaded into storage");
            step();
            require(state().getInt("trial_spawner_state") == 2,
                    "trial treated an unloaded mob as defeated");
            arena.mOwner.loadActorsForChunk(*arena.mLevel, chunkX, chunkZ);
            require(arena.mOwner.getActor(mobId) != nullptr, "trial mob was not restored");
        }
        arena.mOwner.removeActor(mobId);
        for (int i = 0; i < 8 && state().getInt("trial_spawner_state") != 5; ++i)
            step();
        require(state().getInt("trial_spawner_state") == 5, "trial did not finish after the last mob");
        const auto &items = arena.mOwner.getItemEntities();
        require(items.size() == 1, "trial did not eject one reward");
        const ItemStack &item = items.front()->getItem();
        const char *key = ominous ? "minecraft:ominous_trial_key" : "minecraft:trial_key";
        require(item.mDefinition != nullptr && item.mDefinition->getIdentifier() == key,
                "trial ejected the wrong reward");
        require(spawner.saveNbt().getLong("cooldown_end_at") - arena.mLevel->getWorldTick() == cooldown,
                "trial did not start the configured cooldown");
        for (int i = 0; i < cooldown - 1; ++i)
            arena.mLevel->tickTime();
        require(spawner.tick(arena.mOwner) && state().getInt("trial_spawner_state") == 5,
                "trial cooldown ended a tick early");
        step();
        require(state().getInt("trial_spawner_state") == 1 && state().getByte("ominous") == 0,
                "trial cooldown did not restore the normal waiting state");
        arena.mOwner.erasePlayer(player.getNetworkIdentifier());
    }

    void trialSpawnerSlimeClearance(Arena &arena, int32_t variant) {
        arena.clear();
        const Vector3i pos(32, 80, 32);
        arena.spawner(pos, nullptr);
        for (int32_t x = pos.x - 6; x <= pos.x + 6; ++x) {
            for (int32_t z = pos.z - 6; z <= pos.z + 6; ++z)
                arena.mLevel->setBlockState(x, pos.y + 1, z, BlockState("minecraft:stone"));
        }
        Tag states = Tag::ofCompound();
        states.putByte("ominous", 0);
        states.putInt("trial_spawner_state", 1);
        arena.mLevel->setBlockState(pos.x, pos.y, pos.z, BlockState("minecraft:trial_spawner", states));
        auto &spawner = arena.mLevel->getBlockActors().getOrCreate<TrialSpawnerBlockActor>(pos);

        Tag entity = Tag::ofCompound();
        entity.putString("id", "minecraft:slime");
        entity.putInt("Size", variant);
        Tag spawn = Tag::ofCompound();
        spawn.put("entity", entity);
        Tag entry = Tag::ofCompound();
        entry.put("data", spawn);
        entry.putInt("weight", 1);
        Tag potentials = Tag::ofList(Tag::Type::Compound);
        potentials.addToList(entry);
        Tag config = Tag::ofCompound();
        config.put("spawn_potentials", potentials);
        Tag data = Tag::ofCompound();
        data.put("normal_config", config);
        spawner.loadTrialData(TrialSpawnerBlockActor::createTrialData(data));

        ServerPlayer &player = arena.player(9510, Vector3f(34.5f, 79.0f, 32.5f));
        player.setUniqueId(9510);
        for (int i = 0; i < 200 && arena.all("minecraft:slime").empty(); ++i) {
            arena.mLevel->tickTime();
            require(spawner.tick(arena.mOwner), "slime trial spawner removed itself");
        }
        const auto slimes = arena.all("minecraft:slime");
        require(slimes.size() == 1, "configured slime did not fit under a two block ceiling");
        const auto *slime = dynamic_cast<const AbstractSlimeActor *>(slimes.front());
        require(slime != nullptr && slime->getSizeVariant() == variant, "trial spawned the wrong slime size");
        arena.mOwner.erasePlayer(player.getNetworkIdentifier());
        arena.mLevel->getBlockActors().remove(pos);
    }

    void trialSpawnerNamedPreset(Arena &arena, bool ominous, const char *preset, const char *entity) {
        arena.clear();
        const Vector3i position(32, 80, 32);
        arena.spawner(position, nullptr);
        Tag states = Tag::ofCompound();
        states.putByte("ominous", ominous ? 1 : 0);
        states.putInt("trial_spawner_state", 1);
        arena.mLevel->setBlockState(position.x, position.y, position.z, BlockState("minecraft:trial_spawner", states));
        auto &spawner = arena.mLevel->getBlockActors().getOrCreate<TrialSpawnerBlockActor>(position);
        Tag config = Tag::ofCompound();
        const std::string name = std::string("minecraft:trial_chamber/") + preset;
        config.putString("normal_config", name + "/normal");
        config.putString("ominous_config", name + "/ominous");
        spawner.loadTrialData(TrialSpawnerBlockActor::createTrialData(config));

        ServerPlayer &player = arena.player(9507, Vector3f(34.5f, 79.0f, 32.5f));
        player.setUniqueId(9507);
        player.setGameType((int32_t) GameType::Creative);
        require(spawner.tick(arena.mOwner)
                        && spawner.saveNbt().get("registered_players")->getList().empty(),
                "trial spawner detected a creative player");
        player.setGameType((int32_t) GameType::Spectator);
        require(spawner.tick(arena.mOwner)
                        && spawner.saveNbt().get("registered_players")->getList().empty(),
                "trial spawner detected a spectator");
        player.setGameType((int32_t) GameType::Survival);
        arena.mLevel->setBlockState(33, 80, 32, BlockState("minecraft:stone"));
        require(spawner.tick(arena.mOwner)
                        && spawner.saveNbt().get("registered_players")->getList().empty(),
                "trial spawner detected a player through stone");
        arena.mLevel->setBlockState(33, 80, 32, BlockState("minecraft:air"));
        for (int i = 0; i < 100 && arena.all(entity).empty(); ++i) {
            arena.mLevel->tickTime();
            require(spawner.tick(arena.mOwner), "named trial spawner removed itself");
        }
        require(arena.all(entity).size() == 1, "named trial preset did not spawn the configured mob");
        require(spawner.saveNbt().getString("normal_config") == config.getString("normal_config"),
                "named trial preset did not survive a tick");
        require(spawner.saveNbt().getString("ominous_config") == config.getString("ominous_config"),
                "named ominous trial preset did not survive a tick");
        arena.mOwner.erasePlayer(player.getNetworkIdentifier());
    }

    void trialSpawnerEquipment(Arena &arena) {
        arena.clear();
        Tag entity = Tag::ofCompound();
        entity.putString("id", "minecraft:zombie");
        Tag equipment = Tag::ofCompound();
        equipment.putString("loot_table", "minecraft:equipment/trial_chamber_melee");
        equipment.putFloat("slot_drop_chances", 0.0f);
        Tag spawn = Tag::ofCompound();
        spawn.put("entity", entity);
        spawn.put("equipment", equipment);
        ServerActor *actor = TrialSpawnerBlockActor::spawnMob(arena.mOwner, *arena.mLevel,
                                                             Vector3f(32.5f, 80.0f, 32.5f), spawn);
        auto *mob = dynamic_cast<MobActor *>(actor);
        require(mob != nullptr, "trial equipment test did not spawn a mob");
        const ItemStack &weapon = mob->getEquipment().getSlot(MobEquipment::MAINHAND);
        require(weapon.mDefinition != nullptr, "trial mob did not equip a weapon");
        const std::string &weaponId = weapon.mDefinition->getIdentifier();
        require(weaponId == "minecraft:iron_sword" || weaponId == "minecraft:diamond_sword",
                "trial mob did not equip a weapon from its table");

        Tag saved = Tag::ofCompound();
        mob->getEquipment().saveNbt(saved);
        MobEquipment restored;
        restored.loadNbt(saved, arena.mOwner.getCodecContext());
        Tag roundtrip = Tag::ofCompound();
        restored.saveNbt(roundtrip);
        require(roundtrip.getFloat("FalconEquipmentDropChance", -1.0f) == 0.0f,
                "trial equipment drop chance was not restored");
        arena.clear();
    }

    void trialSpawnerOminousPosition(Arena &arena) {
        const Vector3i center(16, 80, 16);
        arena.spawner(center, nullptr);
        ServerPlayer &player = arena.player(9509, Vector3f(18.25f, 79.0f, 16.75f));
        const Vector3f anchor = TrialSpawnerBlockActor::ominousAnchor(player);
        require(anchor.y > 80.0f && anchor.y < 81.0f, "ominous attack did not start above the player's feet");
        const auto open = TrialSpawnerBlockActor::ominousPosition(*arena.mLevel, anchor, 4);
        require(open.has_value() && open->x == anchor.x && open->y == anchor.y + 4.0f && open->z == anchor.z,
                "open ominous attack column did not preserve the anchor");
        arena.mLevel->setBlockState(18, 82, 16, BlockState("minecraft:stone"));
        const auto clipped = TrialSpawnerBlockActor::ominousPosition(*arena.mLevel, anchor, 4);
        require(clipped.has_value() && *clipped == Vector3f(18.5f, 81.5f, 16.5f),
                "ominous attack was not centered in the block below the ceiling");
        arena.mLevel->setBlockState(18, 81, 16, BlockState("minecraft:stone"));
        require(!TrialSpawnerBlockActor::ominousPosition(*arena.mLevel, anchor, 4).has_value(),
                "ominous attack appeared inside the target's block");
        arena.mOwner.erasePlayer(player.getNetworkIdentifier());
    }

    void trialSpawnerProjectiles(Arena &arena) {
        arena.clear();
        const Vector3f position(32.5f, 85.0f, 32.5f);
        std::mt19937 random(42);
        require(!TrialSpawnerBlockActor::rollOminousItems(Tag::ofCompound(), random).empty(),
                "ominous trial did not find the default projectile table");
        Tag custom = Tag::ofCompound();
        custom.putString("items_to_drop_when_ominous", "minecraft:spawners/ominous/trial_chamber/key");
        const auto items = TrialSpawnerBlockActor::rollOminousItems(custom, random);
        require(items.size() == 1 && items.front().mIdentifier == "minecraft:ominous_trial_key",
                "ominous trial ignored the configured item table");

        LootDrop potion;
        potion.mIdentifier = "minecraft:lingering_potion";
        potion.mData = 5;
        auto *potionActor = dynamic_cast<LingeringPotionActor *>(
                TrialSpawnerBlockActor::spawnOminousProjectile(arena.mOwner, *arena.mLevel, position, potion));
        require(potionActor != nullptr && potionActor->getPotionId() == potion.mData,
                "ominous potion lost its effect");

        LootDrop arrow;
        arrow.mIdentifier = "minecraft:arrow";
        arrow.mData = 6;
        auto *arrowActor = dynamic_cast<ArrowActor *>(
                TrialSpawnerBlockActor::spawnOminousProjectile(arena.mOwner, *arena.mLevel, position, arrow));
        require(arrowActor != nullptr && arrowActor->getProjectileData().mPotionId == 5,
                "ominous tipped arrow lost its effect");

        // a wind charge has no gravity: unless it is shot down it hangs where it appeared
        LootDrop wind;
        wind.mIdentifier = "minecraft:wind_charge";
        ServerActor *charge = TrialSpawnerBlockActor::spawnOminousProjectile(arena.mOwner, *arena.mLevel, position, wind);
        require(charge != nullptr && charge->getMotion().y < -0.5f
                        && std::fabs(charge->getMotion().x) < 0.2f && std::fabs(charge->getMotion().z) < 0.2f,
                "ominous wind charge was not shot down");

        const auto keysOnGround = [&arena]() {
            size_t keys = 0;
            for (const auto &item: arena.mOwner.getItemEntities()) {
                if (!item->getItem().isAir()
                    && item->getItem().mDefinition->getIdentifier() == "minecraft:ominous_trial_key")
                    ++keys;
            }
            return keys;
        };
        const size_t keysBefore = keysOnGround();
        LootDrop key;
        key.mIdentifier = "minecraft:ominous_trial_key";
        key.mCount = 1;
        require(TrialSpawnerBlockActor::spawnOminousProjectile(arena.mOwner, *arena.mLevel, position, key) == nullptr,
                "an ominous item that does not shoot became a projectile");
        require(keysOnGround() == keysBefore + 1, "an ominous item that does not shoot was not dropped");
        arena.clear();
    }

    void trialSpawnerEgg(Arena &arena) {
        arena.clear();
        const Vector3i position(40, 80, 16);
        Tag states = Tag::ofCompound();
        states.putByte("ominous", 0);
        states.putInt("trial_spawner_state", 0);
        arena.mLevel->setBlockState(position.x, position.y, position.z, BlockState("minecraft:trial_spawner", states));
        ServerPlayer &player = arena.player(9508, Vector3f(42.5f, 79.0f, 16.5f));
        player.getInventory().setItemInHand(arena.mOwner.createItemStack("minecraft:zombie_spawn_egg", 2));
        const auto egg = ItemClassRegistry::create(VanillaItems::ZOMBIE_SPAWN_EGG());
        require(egg->onUseOnBlock(arena.mOwner, player, player.getInventory().getItemInHand(), position, 1,
                                  Vector3f(0.5f, 1.0f, 0.5f)), "trial spawner did not accept the spawn egg");
        auto *actor = arena.mLevel->getBlockActors().find(position);
        require(dynamic_cast<TrialSpawnerBlockActor *>(actor) != nullptr, "trial spawner actor was not created");
        const Tag client = actor->getSpawnCompound();
        const Tag *display = client.get("spawn_data");
        const Tag *entity = display == nullptr ? nullptr : display->get("entity");
        require(entity != nullptr && entity->getString("id") == "minecraft:zombie",
                "trial spawner did not send the egg mob to the client");
        require(arena.mOwner.getActors().empty() && player.getInventory().getItemInHand().mCount == 1,
                "trial spawner egg did not configure the block without spawning a mob");
        arena.mOwner.erasePlayer(player.getNetworkIdentifier());
    }

    void spawnEgg(Arena &arena) {
        arena.clear();
        const Vector3i position(40, 80, 0);
        MobSpawnerBlockActor &spawner = arena.spawner(position, BLAZE);
        ServerPlayer &player = arena.player(9505, Vector3f(42.5f, 79.0f, 0.5f));
        player.getInventory().setItemInHand(arena.mOwner.createItemStack("minecraft:zombie_spawn_egg", 2));

        const std::unique_ptr<Item> egg = ItemClassRegistry::create(VanillaItems::ZOMBIE_SPAWN_EGG());
        require(egg->onUseOnBlock(arena.mOwner, player, player.getInventory().getItemInHand(), position, 1,
                                  Vector3f(0.5f, 1.0f, 0.5f)),
                "the egg was not used on the spawner");
        require(spawner.getEntityIdentifier() == "minecraft:zombie", "the egg did not change the spawner's mob");
        require(arena.mOwner.getActors().empty(), "the egg spawned a zombie on top of the spawner");
        require(player.getInventory().getItemInHand().mCount == 1, "the egg was not used up");
        arena.mOwner.erasePlayer(player.getNetworkIdentifier());
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-mob-spawner-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        Arena arena(directory);
        defaultsAndSave(arena);
        spawnsNearPlayer(arena);
        stopsAtCap(arena);
        needsSpace(arena);
        emptySpawner(arena);
        spawnEgg(arena);
        arena.mLevel->setOwner(&arena.mOwner);
        generatedVaultUsesPluginFactory(arena);
        vaultCycle(arena, false);
        vaultCycle(arena, true);
        trialSpawnerData(arena);
        trialSpawnerCycle(arena, false, 36000);
        trialSpawnerCycle(arena, true, 36000);
        trialSpawnerCycle(arena, true, 40);
        trialSpawnerSlimeClearance(arena, 1);
        trialSpawnerSlimeClearance(arena, 2);
        trialSpawnerNamedPreset(arena, false, "breeze", "minecraft:breeze");
        trialSpawnerNamedPreset(arena, true, "breeze", "minecraft:breeze");
        trialSpawnerNamedPreset(arena, true, "melee/zombie", "minecraft:zombie");
        trialSpawnerNamedPreset(arena, false, "slow_ranged/poison_skeleton", "minecraft:bogged");
        trialSpawnerEquipment(arena);
        trialSpawnerEgg(arena);
        trialSpawnerOminousPosition(arena);
        trialSpawnerProjectiles(arena);
        require(arena.mLevel->openStorage(directory.string()), "could not open storage for trial unload test");
        trialSpawnerCycle(arena, true, 40, true);
        std::cout << "mob spawner checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
