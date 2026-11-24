#include "World/Level/Block/Actor/TrialSpawnerBlockActor.h"

#include "Network/ServerNetworkHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Protocol/Types/StartGameTypes.h"
#include "Scripting/Binding/ScriptRaycast.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Mob/Hostile/AbstractSlimeActor.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Projectile/ProjectileActor.h"
#include "World/Level/Block/TrialSpawnerBlock.h"
#include "World/Level/Level.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Level/Storage/Loot/LootItems.h"
#include "World/Level/Storage/Loot/LootTableRegistry.h"
#include "World/Level/World.h"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <unordered_map>

namespace {
    std::string resolveLootPath(std::string path) {
        if (path.starts_with("minecraft:"))
            path.erase(0, sizeof("minecraft:") - 1);
        if (!path.empty() && !path.ends_with(".json"))
            path += ".json";
        return path;
    }

    std::vector<LootDrop> rollLoot(std::string path, std::mt19937 &random) {
        const LootTable *table = LootTableRegistry::getInstance().get(resolveLootPath(std::move(path)));
        if (table == nullptr)
            return {};
        LootContext context(random);
        return table->roll(context);
    }

    Tag defaultRewards(bool ominous) {
        Tag rewards = Tag::ofList(Tag::Type::Compound);
        Tag reward = Tag::ofCompound();
        reward.putString("data", ominous ? "minecraft:spawners/ominous/trial_chamber/key"
                                         : "minecraft:spawners/trial_chamber/key");
        reward.putInt("weight", ominous ? 3 : 1);
        rewards.addToList(reward);
        reward.putString("data", ominous ? "minecraft:spawners/ominous/trial_chamber/consumables"
                                         : "minecraft:spawners/trial_chamber/consumables");
        reward.putInt("weight", ominous ? 7 : 1);
        rewards.addToList(reward);
        return rewards;
    }

    struct TrialPreset {
        const char *mName;
        const char *mEntity;
        const char *mEquipment = nullptr;
        bool mBaby = false;
        bool mSlime = false;
    };

    constexpr TrialPreset PRESETS[] = {
            {"breeze", "minecraft:breeze"},
            {"melee/husk", "minecraft:husk", "minecraft:equipment/trial_chamber_melee"},
            {"melee/spider", "minecraft:spider"},
            {"melee/zombie", "minecraft:zombie", "minecraft:equipment/trial_chamber_melee"},
            {"ranged/poison_skeleton", "minecraft:bogged", "minecraft:equipment/trial_chamber_ranged"},
            {"ranged/skeleton", "minecraft:skeleton", "minecraft:equipment/trial_chamber_ranged"},
            {"ranged/stray", "minecraft:stray", "minecraft:equipment/trial_chamber_ranged"},
            {"slow_ranged/poison_skeleton", "minecraft:bogged", "minecraft:equipment/trial_chamber_ranged"},
            {"slow_ranged/skeleton", "minecraft:skeleton", "minecraft:equipment/trial_chamber_ranged"},
            {"slow_ranged/stray", "minecraft:stray", "minecraft:equipment/trial_chamber_ranged"},
            {"small_melee/baby_zombie", "minecraft:zombie", "minecraft:equipment/trial_chamber_melee", true},
            {"small_melee/cave_spider", "minecraft:cave_spider"},
            {"small_melee/silverfish", "minecraft:silverfish"},
            {"small_melee/slime", "minecraft:slime", nullptr, false, true},
    };

    Tag makePreset(const TrialPreset &preset, bool ominous) {
        Tag entity = Tag::ofCompound();
        entity.putString("id", preset.mEntity);
        if (preset.mBaby)
            entity.putByte("IsBaby", 1);
        if (preset.mSlime)
            entity.putInt("Size", 1);

        Tag spawn = Tag::ofCompound();
        spawn.put("entity", entity);
        if (ominous && preset.mEquipment != nullptr) {
            Tag equipment = Tag::ofCompound();
            equipment.putString("loot_table", preset.mEquipment);
            equipment.putFloat("slot_drop_chances", 0.0f);
            spawn.put("equipment", equipment);
        }
        Tag entry = Tag::ofCompound();
        entry.put("data", spawn);
        entry.putInt("weight", preset.mSlime ? 3 : 1);
        Tag potentials = Tag::ofList(Tag::Type::Compound);
        potentials.addToList(entry);
        if (preset.mSlime) {
            entity.putInt("Size", 2);
            spawn.put("entity", entity);
            entry.put("data", spawn);
            entry.putInt("weight", 1);
            potentials.addToList(entry);
        }

        const std::string_view name(preset.mName);
        const bool breeze = name == "breeze";
        const bool slowRanged = name.starts_with("slow_ranged/");
        const bool extraMobs = ominous && preset.mEquipment == nullptr;
        Tag config = Tag::ofCompound();
        config.put("spawn_potentials", potentials);
        config.putInt("ticks_between_spawn", slowRanged ? 160 : 20);
        config.putFloat("simultaneous_mobs", breeze ? (ominous ? 2.0f : 1.0f)
                                                   : (slowRanged || extraMobs ? 4.0f : preset.mBaby ? 2.0f : 3.0f));
        config.putFloat("simultaneous_mobs_added_per_player", slowRanged ? 2.0f : 0.5f);
        config.putFloat("total_mobs", breeze ? (ominous ? 4.0f : 2.0f) : (extraMobs ? 12.0f : 6.0f));
        config.putFloat("total_mobs_added_per_player", breeze ? 1.0f : 2.0f);
        config.put("loot_tables_to_eject", defaultRewards(ominous));
        return config;
    }

    const Tag *resolveConfig(const Tag *stored) {
        if (stored == nullptr || stored->isCompound())
            return stored;
        if (stored->getType() != Tag::Type::String)
            return nullptr;
        static const auto configs = [] {
            std::unordered_map<std::string, Tag> result;
            for (const TrialPreset &preset: PRESETS) {
                const std::string prefix = std::string("minecraft:trial_chamber/") + preset.mName;
                result.emplace(prefix + "/normal", makePreset(preset, false));
                result.emplace(prefix + "/ominous", makePreset(preset, true));
            }
            return result;
        }();
        const auto found = configs.find(stored->asString());
        return found == configs.end() ? nullptr : &found->second;
    }

    bool canSeeSpawner(Level &level, const Vector3i &position, const Vector3f &origin, float range) {
        const Vector3f offset(position.x + 0.5f - origin.x, position.y + 0.5f - origin.y,
                              position.z + 0.5f - origin.z);
        const float distance = std::sqrt(offset.x * offset.x + offset.y * offset.y + offset.z * offset.z);
        if (distance > range)
            return false;
        if (distance == 0.0f)
            return true;

        ScriptRaycast::BlockOptions options;
        options.mMaxDistance = distance;
        ScriptRaycast::BlockHit hit;
        const Vector3f direction(offset.x / distance, offset.y / distance, offset.z / distance);
        return !ScriptRaycast::castBlocks(level, origin, direction, options, hit) || hit.mBlock == position;
    }

    bool detectsPlayer(Level &level, const Vector3i &position, const ServerPlayer &player, float range) {
        if (!player.isSpawned() || player.isDead() || !player.isIn(level))
            return false;
        const auto mode = (GameType) player.getGameType();
        if (mode == GameType::Creative || mode == GameType::Spectator)
            return false;

        Vector3f eye = player.getPosition();
        eye.y += MovementPhysics::PLAYER_BASE_OFFSET;
        return canSeeSpawner(level, position, eye, range);
    }

    const Tag *drawWeighted(const Tag *entries, std::mt19937 &random) {
        if (entries == nullptr || !entries->isList())
            return nullptr;

        int64_t total = 0;
        for (const Tag &entry: entries->getList())
            total += std::max(0, entry.getInt("weight"));
        if (total == 0)
            return nullptr;

        int64_t draw = std::uniform_int_distribution<int64_t>(0, total - 1)(random);
        for (const Tag &entry: entries->getList()) {
            draw -= std::max(0, entry.getInt("weight"));
            if (draw < 0)
                return entry.get("data");
        }
        return nullptr;
    }
}

Tag TrialSpawnerBlockActor::saveNbt() const {
    return mTrialData;
}

Tag TrialSpawnerBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    if (const Tag *spawn = mTrialData.get("spawn_data"); spawn != nullptr && spawn->isCompound())
        data.put("spawn_data", *spawn);
    return data;
}

void TrialSpawnerBlockActor::loadNbt(const Tag &data, const PacketCodecContext &) {
    loadTrialData(data);
}

void TrialSpawnerBlockActor::loadTrialData(const Tag &data) {
    mTrialData = data;
    for (const char *key: {"id", "x", "y", "z", "DynamicProperties", "BlockEntityStorage"})
        mTrialData.remove(key);
}

bool TrialSpawnerBlockActor::configureFromSpawnEgg(ServerNetworkHandler &owner, const std::string &identifier) {
    if (mLevel == nullptr || identifier.empty())
        return false;
    const BlockState *state = mLevel->peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
    if (state == nullptr || state->mStates.getInt("trial_spawner_state") != 0)
        return false;

    Tag entity = Tag::ofCompound();
    entity.putString("id", identifier);
    Tag spawn = Tag::ofCompound();
    spawn.put("entity", entity);
    Tag potential = Tag::ofCompound();
    potential.put("data", spawn);
    potential.putInt("weight", 1);
    Tag potentials = Tag::ofList(Tag::Type::Compound);
    potentials.addToList(potential);

    Tag normal = Tag::ofCompound();
    normal.put("spawn_potentials", potentials);
    normal.put("loot_tables_to_eject", defaultRewards(false));
    Tag ominous = normal;
    ominous.put("loot_tables_to_eject", defaultRewards(true));
    Tag config = Tag::ofCompound();
    config.put("normal_config", normal);
    config.put("ominous_config", ominous);
    mTrialData = createTrialData(config);
    mTrialData.put("spawn_data", spawn);

    Tag states = state->mStates;
    states.putInt("trial_spawner_state", 1);
    mLevel->setBlock(mPosition, BlockState(state->mName, states), true);
    BlockActionHandler::broadcastBlockActorData(owner, *mLevel, *this);
    return true;
}

bool TrialSpawnerBlockActor::tick(ServerNetworkHandler &owner) {
    if (mLevel == nullptr)
        return true;
    const BlockState *state = mLevel->peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
    if (state == nullptr)
        return true;
    if (!TrialSpawnerBlock::matches(state->mName))
        return false;

    constexpr int32_t WAITING_FOR_PLAYERS = 1;
    constexpr int32_t ACTIVE = 2;
    constexpr int32_t WAITING_FOR_REWARD = 3;
    constexpr int32_t EJECTING_REWARD = 4;
    constexpr int32_t COOLDOWN = 5;
    const int32_t phase = state->mStates.getInt("trial_spawner_state");
    const bool ominous = state->mStates.getByte("ominous") != 0;
    const Tag *config = resolveConfig(mTrialData.get(ominous ? "ominous_config" : "normal_config"));
    const Tag *spawnConfig = config;
    // some chamber templates only override waves and rewards in their ominous compound
    if (ominous && config != nullptr && !config->contains("spawn_potentials"))
        spawnConfig = resolveConfig(mTrialData.get("normal_config"));

    World *world = owner.getWorlds().find(mLevel->getName());
    const int64_t now = world == nullptr ? mLevel->getWorldTick() : world->getOverworld().getWorldTick();
    const Vector3f center(mPosition.x + 0.5f, mPosition.y + 0.5f, mPosition.z + 0.5f);
    const auto playSound = [&](const char *sound) {
        owner.playLevelSound(*mLevel, sound, center);
    };
    const auto setPhase = [&](int32_t next) {
        Tag states = state->mStates;
        states.putInt("trial_spawner_state", next);
        mLevel->setBlock(mPosition, BlockState(state->mName, states), true);
    };
    if (!ominous && (phase == WAITING_FOR_PLAYERS || phase == ACTIVE || phase == COOLDOWN)
        && mTrialData.contains("ominous_config")) {
        const float range = (float) mTrialData.getInt("required_player_range", 14);
        for (auto &entry: owner.getPlayers()) {
            ServerPlayer &player = entry.second;
            const MobEffectInstance *badOmen = player.getEffect(MobEffectId::BadOmen);
            if (badOmen == nullptr && !player.hasEffect(MobEffectId::TrialOmen))
                continue;
            if (!detectsPlayer(*mLevel, mPosition, player, range))
                continue;

            if (badOmen != nullptr) {
                constexpr int32_t TICKS_PER_OMEN_LEVEL = 15 * 60 * 20;
                const int32_t duration = TICKS_PER_OMEN_LEVEL * (std::clamp(badOmen->mAmplifier, 0, 255) + 1);
                player.removeEffect(MobEffectId::BadOmen);
                owner.addEffect(player, MobEffectId::TrialOmen, 0, duration);
            }

            if (const Tag *mobs = mTrialData.get("current_mobs"); mobs != nullptr && mobs->isList()) {
                for (const Tag &mob: mobs->getList()) {
                    const int64_t id = mob.getLong("uuid");
                    ServerActor *tracked = owner.getActor(id);
                    if (tracked != nullptr && tracked->isIn(*mLevel))
                        owner.removeActor(id);
                    else if (mLevel->hasStoredEntity(id))
                        mLevel->eraseEntity(id);
                }
            }
            mTrialData.remove("spawn_data");
            mTrialData.put("current_mobs", Tag::ofList(Tag::Type::Compound));
            mTrialData.put("registered_players", Tag::ofList(Tag::Type::Compound));
            mTrialData.putInt("total_mobs_spawned", 0);
            mTrialData.putLong("cooldown_end_at", 0);
            mTrialData.putLong("next_mob_spawns_at", 0);

            Tag states = state->mStates;
            playSound("trial_spawner.charge_activate");
            states.putByte("ominous", 1);
            states.putInt("trial_spawner_state", WAITING_FOR_PLAYERS);
            mLevel->setBlock(mPosition, BlockState(state->mName, states), true);
            return true;
        }
    }
    if (phase == WAITING_FOR_PLAYERS || phase == ACTIVE) {
        if (config == nullptr || spawnConfig == nullptr || !spawnConfig->contains("spawn_potentials", Tag::Type::List))
            return true;

        if (!mTrialData.contains("spawn_data", Tag::Type::Compound)) {
            const Tag spawn = rollSpawnData(*spawnConfig, mRandom);
            if (spawn.isCompound()) {
                mTrialData.put("spawn_data", spawn);
                BlockActionHandler::broadcastBlockActorData(owner, *mLevel, *this);
            }
        }

        const float range = (float) mTrialData.getInt("required_player_range", 14);
        const Tag *savedPlayers = mTrialData.get("registered_players");
        Tag players = savedPlayers != nullptr && savedPlayers->isList()
                              ? *savedPlayers : Tag::ofList(Tag::Type::Compound);
        bool added = false;
        for (const auto &entry: owner.getPlayers()) {
            const int64_t id = entry.second.getUniqueId();
            if (std::any_of(players.getList().begin(), players.getList().end(),
                            [id](const Tag &player) {
                                return player.getLong("uuid") == id;
                            }))
                continue;
            if (!detectsPlayer(*mLevel, mPosition, entry.second, range))
                continue;
            Tag player = Tag::ofCompound();
            player.putLong("uuid", id);
            players.addToList(player);
            added = true;
        }
        if (added) {
            mTrialData.put("registered_players", players);
            // the detection event carries the player count and plays its own sound
            constexpr int32_t DETECT_PLAYERS = 3611;
            constexpr int32_t DETECT_PLAYERS_OMINOUS = 3615;
            owner.broadcastLevelEvent(*mLevel, ominous ? DETECT_PLAYERS_OMINOUS : DETECT_PLAYERS,
                                      center, (int32_t) players.getList().size());
        }
        if (phase == WAITING_FOR_PLAYERS) {
            if (!players.getList().empty())
                setPhase(ACTIVE);
            return true;
        }
    }
    if (phase == ACTIVE) {
        if (config == nullptr || !config->isCompound())
            return true;

        const Tag *players = mTrialData.get("registered_players");
        const float extra = players != nullptr && players->isList() && !players->getList().empty()
                                    ? (float) (players->getList().size() - 1) : 0.0f;
        const auto limit = [&](const char *base, float initial, const char *perPlayer, float increment) {
            const float value = config->getFloat(base, initial) + extra * config->getFloat(perPlayer, increment);
            return std::isfinite(value) ? (int32_t) std::clamp(value, 0.0f, 65535.0f) : 0;
        };
        const int32_t total = limit("total_mobs", 6.0f, "total_mobs_added_per_player", 2.0f);
        const int32_t simultaneous = limit("simultaneous_mobs", 2.0f, "simultaneous_mobs_added_per_player", 1.0f);
        if (total == 0 || simultaneous == 0)
            return true;

        Tag mobs = Tag::ofList(Tag::Type::Compound);
        if (const Tag *tracked = mTrialData.get("current_mobs"); tracked != nullptr && tracked->isList()) {
            for (const Tag &entry: tracked->getList()) {
                const int64_t id = entry.getLong("uuid");
                const ServerActor *mob = owner.getActor(id);
                if (mob != nullptr && mob->isIn(*mLevel)) {
                    if (mob->isAlive())
                        mobs.addToList(entry);
                } else if (mLevel->hasStoredEntity(id)) {
                    mobs.addToList(entry);
                }
            }
        }
        mTrialData.put("current_mobs", mobs);

        if (ominous && !mobs.getList().empty()) {
            constexpr int64_t ATTACK_INTERVAL = 160;
            constexpr int32_t MIN_WINDUP = 60;
            constexpr int32_t MAX_WINDUP = 120;
            constexpr int64_t WARNING_TICKS = 35;
            const int64_t release = mTrialData.getLong("FalconOminousRelease");
            if (release != 0) {
                const Vector3f position(mTrialData.getFloat("FalconOminousX"), mTrialData.getFloat("FalconOminousY"),
                                        mTrialData.getFloat("FalconOminousZ"));
                if (now == release - WARNING_TICKS)
                    owner.playLevelSound(*mLevel, "ominous_item_spawner.about_to_spawn_item", position);
                if (now >= release) {
                    const auto drops = rollOminousItems(*config, mRandom);
                    if (!drops.empty()) {
                        const size_t index = std::uniform_int_distribution<size_t>(0, drops.size() - 1)(mRandom);
                        spawnOminousProjectile(owner, *mLevel, position, drops[index]);
                        owner.playLevelSound(*mLevel, "ominous_item_spawner.spawn_item", position);
                    }
                    mTrialData.remove("FalconOminousRelease");
                }
            } else if (now % ATTACK_INTERVAL == 0) {
                std::vector<Vector3f> candidates;
                if (players != nullptr && players->isList()) {
                    for (const auto &entry: owner.getPlayers()) {
                        const ServerPlayer &player = entry.second;
                        if (!player.isSpawned() || player.isDead() || !player.isIn(*mLevel))
                            continue;
                        const auto mode = (GameType) player.getGameType();
                        if (mode == GameType::Creative || mode == GameType::Spectator)
                            continue;
                        const Vector3f &feet = player.getPosition();
                        const float dx = feet.x - center.x;
                        const float dy = feet.y - center.y;
                        const float dz = feet.z - center.z;
                        const float range = (float) mTrialData.getInt("required_player_range", 14);
                        if (dx * dx + dy * dy + dz * dz >= range * range)
                            continue;
                        for (const Tag &registered: players->getList()) {
                            if (registered.getLong("uuid") == player.getUniqueId()) {
                                candidates.push_back(ominousAnchor(player));
                                break;
                            }
                        }
                    }
                }
                if (!candidates.empty() && std::bernoulli_distribution{}(mRandom)) {
                    candidates.clear();
                    const float range = (float) mTrialData.getInt("required_player_range", 14);
                    for (const Tag &entry: mobs.getList()) {
                        const ServerActor *mob = owner.getActor(entry.getLong("uuid"));
                        if (mob != nullptr && mob->isAlive() && mob->isIn(*mLevel)) {
                            const Vector3f &feet = mob->getPosition();
                            const float dx = feet.x - center.x;
                            const float dy = feet.y - center.y;
                            const float dz = feet.z - center.z;
                            if (dx * dx + dy * dy + dz * dz < range * range)
                                candidates.push_back(ominousAnchor(*mob));
                        }
                    }
                }
                if (!candidates.empty()) {
                    const size_t index = std::uniform_int_distribution<size_t>(0, candidates.size() - 1)(mRandom);
                    const auto position = ominousPosition(*mLevel, candidates[index],
                                                          std::uniform_int_distribution<int32_t>(2, 5)(mRandom));
                    if (position.has_value()) {
                        const Vector3f &target = *position;
                        mTrialData.putFloat("FalconOminousX", target.x);
                        mTrialData.putFloat("FalconOminousY", target.y);
                        mTrialData.putFloat("FalconOminousZ", target.z);
                        mTrialData.putLong("FalconOminousRelease", now + std::uniform_int_distribution<int32_t>(MIN_WINDUP, MAX_WINDUP)(mRandom));
                        owner.spawnParticleEffect(*mLevel, "minecraft:ominous_spawning_particle", target);
                        owner.playLevelSound(*mLevel, "ominous_item_spawner.spawn_item_begin", target);
                    }
                }
            }
        }

        const int32_t spawned = mTrialData.getInt("total_mobs_spawned");
        if (spawned >= total) {
            if (mobs.getList().empty()) {
                mTrialData.remove("FalconOminousRelease");
                setPhase(WAITING_FOR_REWARD);
            }
            return true;
        }
        if (mobs.getList().size() >= (size_t) simultaneous || now < mTrialData.getLong("next_mob_spawns_at"))
            return true;

        const int32_t range = std::clamp(config->getInt("spawn_range", 4), 1, 16);
        const int32_t x = mPosition.x + std::uniform_int_distribution<int32_t>(-range, range)(mRandom);
        const int32_t y = mPosition.y + std::uniform_int_distribution<int32_t>(-1, 1)(mRandom);
        const int32_t z = mPosition.z + std::uniform_int_distribution<int32_t>(-range, range)(mRandom);
        if (mLevel->peekBlockPtr(x, y, z) == nullptr)
            return true;

        if (!canSeeSpawner(*mLevel, mPosition, Vector3f(x + 0.5f, y + 0.5f, z + 0.5f),
                           std::sqrt(2.0f * range * range + 2.25f)))
            return true;

        const Tag *spawnData = mTrialData.get("spawn_data");
        const Tag *entity = spawnData == nullptr ? nullptr : spawnData->get("entity");
        if (entity == nullptr || !entity->isCompound())
            return true;
        const std::string identifier = entity->getString("id");
        if (identifier.empty())
            return true;
        ActorSize size = ActorClassRegistry::getSize(identifier);
        if (entity->contains("Size", Tag::Type::Int)
            && dynamic_cast<const AbstractSlimeActor *>(ActorClassRegistry::getPrototype(identifier)) != nullptr)
            size = AbstractSlimeActor::getSizeForVariant(entity->getInt("Size"));
        const float halfWidth = size.mWidth * 0.5f;
        const Vector3f feet(x + 0.5f, (float) y, z + 0.5f);
        const AxisAlignedBB box(feet.x - halfWidth, feet.y, feet.z - halfWidth, feet.x + halfWidth,
                                feet.y + size.mHeight, feet.z + halfWidth);
        if (!mLevel->getCollisionBoxes(box).empty())
            return true;

        ServerActor *mob = spawnMob(owner, *mLevel, feet, *spawnData);
        if (mob != nullptr) {
            Tag entry = Tag::ofCompound();
            entry.putLong("uuid", mob->getUniqueId());
            mobs.addToList(entry);
            constexpr int32_t MOB_SPAWN_PARTICLES = 3612;
            constexpr int32_t SPAWNER_FLAME_PARTICLES = 3613;
            owner.broadcastLevelEvent(*mLevel, MOB_SPAWN_PARTICLES, Vector3f((float) x, (float) y, (float) z), ominous);
            owner.broadcastLevelEvent(*mLevel, SPAWNER_FLAME_PARTICLES, center, ominous);
            playSound("trial_spawner.spawn_mob");
            mTrialData.put("current_mobs", mobs);
            mTrialData.putInt("total_mobs_spawned", spawned + 1);
            mTrialData.remove("spawn_data");
            mTrialData.putLong("next_mob_spawns_at", now + std::max(1, config->getInt("ticks_between_spawn", 20)));
        }
        return true;
    }
    if (phase == WAITING_FOR_REWARD) {
        playSound("trial_spawner.open_shutter");
        setPhase(EJECTING_REWARD);
        return true;
    }
    if (phase == EJECTING_REWARD) {
        const Tag *players = mTrialData.get("registered_players");
        if (players != nullptr && players->isList() && !players->getList().empty()) {
            if (config != nullptr && config->isCompound()) {
                const Vector3f opening(mPosition.x + 0.5f, mPosition.y + 1.0f, mPosition.z + 0.5f);
                for (const LootDrop &drop: rollReward(*config, mRandom)) {
                    const ItemStack item = LootItems::toItemStack(owner, drop);
                    if (item.mCount > 0)
                        owner.dropItem(*mLevel, opening, item, Vector3f(), ItemActorHandler::DROP_PICKUP_DELAY);
                }
            }
            playSound("trial_spawner.eject_item");
            Tag remaining = Tag::ofList(Tag::Type::Compound);
            for (size_t index = 1; index < players->getList().size(); ++index)
                remaining.addToList(players->getList()[index]);
            mTrialData.put("registered_players", remaining);
            return true;
        }

        constexpr int32_t DEFAULT_COOLDOWN_TICKS = 30 * 60 * 20;
        const int32_t cooldown = config == nullptr ? DEFAULT_COOLDOWN_TICKS
                                                   : config->getInt("target_cooldown_length", DEFAULT_COOLDOWN_TICKS);
        mTrialData.putLong("cooldown_end_at", now + std::max(0, cooldown));
        mTrialData.putLong("next_mob_spawns_at", 0);
        mTrialData.putInt("total_mobs_spawned", 0);
        playSound("trial_spawner.close_shutter");
        setPhase(COOLDOWN);
        return true;
    }
    if (phase != COOLDOWN)
        return true;

    if (now < mTrialData.getLong("cooldown_end_at"))
        return true;

    Tag states = state->mStates;
    states.putInt("trial_spawner_state", WAITING_FOR_PLAYERS);
    states.putByte("ominous", 0);
    mTrialData.putLong("cooldown_end_at", 0);
    mLevel->setBlock(mPosition, BlockState(state->mName, states), true);
    return true;
}

Tag TrialSpawnerBlockActor::createTrialData(const Tag &configuration) {
    Tag data = Tag::ofCompound();
    for (const char *key: {"normal_config", "ominous_config"}) {
        if (const Tag *config = configuration.get(key))
            data.put(key, *config);
    }
    data.putLong("cooldown_end_at", 0);
    data.put("current_mobs", Tag::ofList(Tag::Type::Compound));
    data.putLong("next_mob_spawns_at", 0);
    data.put("registered_players", Tag::ofList(Tag::Type::Compound));
    data.putInt("required_player_range", 14);
    data.putInt("total_mobs_spawned", 0);
    return data;
}

std::vector<LootDrop> TrialSpawnerBlockActor::rollReward(const Tag &config, std::mt19937 &random) {
    static const Tag defaultTables = defaultRewards(false);
    const Tag *tables = config.get("loot_tables_to_eject");
    const Tag *selected = drawWeighted(tables == nullptr ? &defaultTables : tables, random);
    if (selected == nullptr || selected->getType() != Tag::Type::String)
        return {};

    return rollLoot(selected->asString(), random);
}

std::vector<LootDrop> TrialSpawnerBlockActor::rollOminousItems(const Tag &config, std::mt19937 &random) {
    return rollLoot(config.getString("items_to_drop_when_ominous",
                                     "loot_tables/spawners/trial_chamber/items_to_drop_when_ominous.json"), random);
}

Tag TrialSpawnerBlockActor::rollSpawnData(const Tag &config, std::mt19937 &random) {
    const Tag *selected = drawWeighted(config.get("spawn_potentials"), random);
    if (selected == nullptr || !selected->isCompound())
        return Tag();
    return *selected;
}

ServerActor *TrialSpawnerBlockActor::spawnMob(ServerNetworkHandler &owner, Level &level, const Vector3f &position,
                                             const Tag &spawnData) {
    const Tag *entity = spawnData.get("entity");
    if (entity == nullptr || !entity->isCompound())
        return nullptr;
    const std::string identifier = entity->getString("id");
    if (identifier.empty())
        return nullptr;

    ServerActor *spawned = owner.spawnActor(level, identifier, position, [entity](ServerActor &actor) {
        actor.setPersistent(true);
        if (entity->getByte("IsBaby") != 0) {
            if (auto *mob = dynamic_cast<MobActor *>(&actor))
                mob->setSpawnEvent("minecraft:as_baby");
        }
        if (entity->contains("Size", Tag::Type::Int)) {
            if (auto *slime = dynamic_cast<AbstractSlimeActor *>(&actor))
                slime->setSizeVariant(entity->getInt("Size"));
        }
    });
    auto *mob = dynamic_cast<MobActor *>(spawned);
    const Tag *equipment = spawnData.get("equipment");
    if (mob != nullptr && equipment != nullptr && equipment->isCompound()) {
        const std::string table = resolveLootPath(equipment->getString("loot_table"));
        if (!table.empty())
            mob->useSpawnEquipment(owner, table, equipment->getFloat("slot_drop_chances"));
    }
    return spawned;
}

Vector3f TrialSpawnerBlockActor::ominousAnchor(const Actor &target) {
    Vector3f anchor = target.getPosition();
    if (target.isPlayer())
        anchor.y += MovementPhysics::PLAYER_BASE_OFFSET;
    else if (const auto *mob = dynamic_cast<const ServerActor *>(&target))
        anchor.y += mob->getSize().mHeight * 0.85f;
    return anchor;
}

std::optional<Vector3f> TrialSpawnerBlockActor::ominousPosition(Level &level, const Vector3f &anchor, int32_t height) {
    ScriptRaycast::BlockOptions options;
    options.mMaxDistance = (float) height;
    ScriptRaycast::BlockHit hit;
    Vector3f position(anchor.x, anchor.y + height, anchor.z);
    if (ScriptRaycast::castBlocks(level, anchor, Vector3f(0.0f, 1.0f, 0.0f), options, hit))
        position = Vector3f(hit.mBlock.x + 0.5f, hit.mBlock.y - 0.5f, hit.mBlock.z + 0.5f);

    const Vector3i block((int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                         (int32_t) std::floor(position.z));
    if (block.y == (int32_t) std::floor(anchor.y) || level.peekBlockPtr(block.x, block.y, block.z) == nullptr)
        return std::nullopt;
    return position;
}

ServerActor *TrialSpawnerBlockActor::spawnOminousProjectile(ServerNetworkHandler &owner, Level &level,
                                                            const Vector3f &position, const LootDrop &drop) {
    std::string identifier = drop.mIdentifier;
    if (identifier == "minecraft:wind_charge")
        identifier = "minecraft:wind_charge_projectile";
    else if (identifier == "minecraft:fire_charge")
        identifier = "minecraft:small_fireball";
    else if (identifier != "minecraft:arrow" && identifier != "minecraft:lingering_potion")
        identifier.clear();

    // the game hands the item to its dispense behaviour aimed straight down, and drops what does not shoot
    if (identifier.empty()) {
        const ItemStack item = LootItems::toItemStack(owner, drop);
        if (item.mCount > 0)
            owner.dropItem(level, position, item, Vector3f(), ItemActorHandler::DROP_PICKUP_DELAY);
        return nullptr;
    }
    return owner.spawnDispensedProjectile(level, identifier, position, Vector3f(0.0f, -1.0f, 0.0f),
                                          [&drop](ServerActor &actor) {
                                              if (auto *potion = dynamic_cast<PotionActor *>(&actor))
                                                  potion->setPotionId(drop.mData);
                                              else if (dynamic_cast<ArrowActor *>(&actor) != nullptr)
                                                  actor.getProjectileData().mPotionId = drop.mData - 1;
                                          });
}
