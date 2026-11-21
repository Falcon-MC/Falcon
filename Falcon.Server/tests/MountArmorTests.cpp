#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ArmorProtection.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/Container.h"
#include "World/Item/BodyArmor.h"
#include "World/Item/ItemProperties.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void near(float actual, float expected, const char *message) {
        require(std::isfinite(actual) && std::fabs(actual - expected) < 0.0001f, message);
    }

    ItemStack stack(const char *identifier) {
        ItemStack item;
        item.mDefinition = std::make_shared<ItemDefinition>(identifier, 1, false, Tag());
        item.mCount = 1;
        return item;
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Mount armor", "", 1};
        Level *mLevel = nullptr;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "normal.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -1; x <= 1; x++) {
                for (int32_t z = -1; z <= 1; z++)
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
        }

        MobActor &spawn(const char *identifier, const char *event) {
            ServerActor *actor = mOwner.spawnActor(*mLevel, identifier, Vector3f(0.5f, 100.0f, 0.5f),
                                                   [event](ServerActor &spawned) {
                                                       static_cast<MobActor &>(spawned).setSpawnEvent(event);
                                                   });
            MobActor *mob = dynamic_cast<MobActor *>(actor);
            require(mob != nullptr, "mob did not spawn");
            mob->startDefinition(mOwner);
            return *mob;
        }

        ServerPlayer &player(uint64_t runtimeId) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(*mLevel);
            player.setPosition(Vector3f(1.5f, 100.0f, 0.5f));
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            return player;
        }
    };

    void armorValues() {
        const struct {
            const char *mIdentifier;
            int32_t mPoints;
            int32_t mToughness;
            float mKnockback;
        } cases[] = {
                {"minecraft:leather_horse_armor", 3, 0, 0.0f},    {"minecraft:copper_horse_armor", 4, 0, 0.0f},
                {"minecraft:iron_horse_armor", 5, 0, 0.0f},       {"minecraft:golden_horse_armor", 7, 0, 0.0f},
                {"minecraft:diamond_horse_armor", 11, 2, 0.0f},   {"minecraft:netherite_horse_armor", 19, 3, 0.1f},
                {"minecraft:copper_nautilus_armor", 4, 0, 0.0f},  {"minecraft:iron_nautilus_armor", 5, 0, 0.0f},
                {"minecraft:golden_nautilus_armor", 7, 0, 0.0f},  {"minecraft:diamond_nautilus_armor", 11, 2, 0.0f},
                {"minecraft:netherite_nautilus_armor", 19, 3, 0.1f}, {"minecraft:wolf_armor", 11, 0, 0.0f},
        };
        for (const auto &entry: cases) {
            const BodyArmorData *data = BodyArmor::find(entry.mIdentifier);
            require(data != nullptr, "a body armor is missing from the table");
            require(data->mPoints == entry.mPoints && data->mToughness == entry.mToughness,
                    "a body armor has the wrong points or toughness");
            require(ArmorProtection::getPoints(stack(entry.mIdentifier)) == entry.mPoints,
                    "the protection skips a body armor");
            near(ItemProperties::getArmorKnockbackResistance(entry.mIdentifier), entry.mKnockback,
                 "a body armor has the wrong knockback resistance");
            require(data->mWearsOnHit == (std::string(entry.mIdentifier) == "minecraft:wolf_armor"),
                    "only the wolf armor wears down");
        }
        require(BodyArmor::find("minecraft:iron_chestplate") == nullptr, "a chestplate is body armor");
        require(ArmorProtection::getPoints(stack("minecraft:iron_chestplate")) == 6, "the chestplate lost its points");
    }

    void bodyProtection(Arena &arena) {
        MobActor &horse = arena.spawn("minecraft:horse", "minecraft:spawn_adult");
        const ActorDamageSource source = ActorDamageSource::environment("death.attack.generic", "horse");
        near(horse.absorbDamage(10.0f, source), 10.0f, "a bare horse absorbed damage");

        horse.getEquipment().setSlot(MobEquipment::BODY, stack("minecraft:diamond_horse_armor"));
        near(horse.absorbDamage(10.0f, source), 10.0f * (1.0f - 11 * 0.04f), "diamond horse armor gave no 11 points");
        near(horse.getArmorKnockbackResistance(), 0.0f, "diamond horse armor resists knockback");

        horse.getEquipment().setSlot(MobEquipment::BODY, stack("minecraft:netherite_horse_armor"));
        near(horse.absorbDamage(10.0f, source), 10.0f * (1.0f - 19 * 0.04f), "netherite horse armor gave no 19 points");
        near(horse.getArmorKnockbackResistance(), 0.1f, "netherite horse armor resists no knockback");

        const ActorDamageSource fall = ActorDamageSource::environment("death.fell.accident.generic", "horse");
        near(horse.absorbDamage(10.0f, fall), 10.0f, "horse armor reduced fall damage");
    }

    void armorWear(Arena &arena) {
        MobActor &wolf = arena.spawn("minecraft:wolf", "minecraft:spawn_wild_adult");
        wolf.getEquipment().setSlot(MobEquipment::BODY, stack("minecraft:wolf_armor"));
        arena.mOwner.hurtActor(wolf, 6.0f, "death.attack.generic");
        require(wolf.getEquipment().getSlot(MobEquipment::BODY).mDamage == 1, "a light hit wore no single point");

        wolf.setNoDamageTicks(0);
        arena.mOwner.hurtActor(wolf, 3.0f, "death.attack.drown");
        require(wolf.getEquipment().getSlot(MobEquipment::BODY).mDamage == 1, "drowning wore the wolf armor");

        ItemStack worn = stack("minecraft:wolf_armor");
        worn.mDamage = 63;
        wolf.getEquipment().setSlot(MobEquipment::BODY, worn);
        wolf.setNoDamageTicks(0);
        arena.mOwner.hurtActor(wolf, 1.0f, "death.attack.generic");
        require(wolf.getEquipment().getSlot(MobEquipment::BODY).isAir(), "worn out wolf armor did not break");

        MobActor &horse = arena.spawn("minecraft:horse", "minecraft:spawn_adult");
        horse.getEquipment().setSlot(MobEquipment::BODY, stack("minecraft:iron_horse_armor"));
        arena.mOwner.hurtActor(horse, 8.0f, "death.attack.generic");
        const ItemStack &armor = horse.getEquipment().getSlot(MobEquipment::BODY);
        require(!armor.isAir() && armor.mDamage == 0, "horse armor wore down");
    }

    void wolfArmorInteractions(Arena &arena) {
        MobActor &wolf = arena.spawn("minecraft:wolf", "minecraft:spawn_wild_adult");
        ServerPlayer &stranger = arena.player(9001);
        ServerPlayer &owner = arena.player(9002);
        require(wolf.tameTo(arena.mOwner, owner, "minecraft:tameable", false), "the wolf was not tamed");
        wolf.fireEvent(arena.mOwner, "minecraft:become_armorable");

        stranger.getInventory().setItemInHand(stack("minecraft:wolf_armor"));
        wolf.onInteract(arena.mOwner, stranger);
        require(wolf.getEquipment().getSlot(MobEquipment::BODY).isAir(), "a stranger armored the wolf");

        owner.getInventory().setItemInHand(stack("minecraft:wolf_armor"));
        wolf.onInteract(arena.mOwner, owner);
        require(wolf.getEquipment().getSlot(MobEquipment::BODY).mDefinition != nullptr
                        && wolf.getEquipment().getSlot(MobEquipment::BODY).mDefinition->getIdentifier()
                                   == "minecraft:wolf_armor",
                "the owner could not armor the wolf");
        require(owner.getInventory().getItemInHand().isAir(), "equipping kept the wolf armor in hand");

        require(!wolf.getEquipment().repairSlot(MobEquipment::BODY, 8), "an undamaged armor was repaired");
        ItemStack worn = stack("minecraft:wolf_armor");
        worn.mDamage = 20;
        wolf.getEquipment().setSlot(MobEquipment::BODY, worn);
        require(wolf.getEquipment().repairSlot(MobEquipment::BODY, 8)
                        && wolf.getEquipment().getSlot(MobEquipment::BODY).mDamage == 12,
                "a scute did not give back 8 points");
        require(wolf.getEquipment().repairSlot(MobEquipment::BODY, 50)
                        && wolf.getEquipment().getSlot(MobEquipment::BODY).mDamage == 0,
                "a repair went past full durability");
    }

    void bodySaved(Arena &arena) {
        MobActor &horse = arena.spawn("minecraft:horse", "minecraft:spawn_adult");
        horse.getEquipment().setSlot(MobEquipment::BODY, stack("minecraft:golden_horse_armor"));
        Tag saved = Tag::ofCompound();
        horse.getEquipment().saveNbt(saved);
        const Tag *armor = saved.get("Armor");
        require(armor != nullptr && armor->getType() == Tag::Type::List && armor->getList().size() == 5,
                "the armor list lost its body entry");

        MobActor &copy = arena.spawn("minecraft:horse", "minecraft:spawn_adult");
        copy.getEquipment().loadNbt(saved, arena.mOwner.getCodecContext());
        const ItemStack &loaded = copy.getEquipment().getSlot(MobEquipment::BODY);
        require(loaded.mDefinition != nullptr && loaded.mDefinition->getIdentifier() == "minecraft:golden_horse_armor",
                "the body armor was not loaded back");
    }

    bool holds(const Container &container, int slot, const char *identifier) {
        const ItemStack &item = container.getContainerItem(slot);
        return item.mDefinition != nullptr && item.mDefinition->getIdentifier() == identifier;
    }

    void mountWindow(Arena &arena) {
        MobActor &horse = arena.spawn("minecraft:horse", "minecraft:spawn_adult");
        ServerPlayer &rider = arena.player(9003);
        require(horse.getEntityContainer() == nullptr, "a wild horse opened its window");
        require(horse.tameTo(arena.mOwner, rider, "minecraft:tamemount", false), "the horse was not tamed");
        Container *window = horse.getEntityContainer();
        require(window != nullptr && window->getContainerSize() == 2, "the tamed horse window is not two slots");

        require(window->canHoldInSlot(0, stack("minecraft:saddle")), "the saddle slot refused a saddle");
        require(!window->canHoldInSlot(0, stack("minecraft:iron_horse_armor")), "the saddle slot took armor");
        require(window->canHoldInSlot(1, stack("minecraft:iron_horse_armor")), "the armor slot refused iron armor");
        require(window->canHoldInSlot(1, stack("minecraft:netherite_horse_armor")),
                "the armor slot refused netherite armor");
        require(!window->canHoldInSlot(1, stack("minecraft:saddle")), "the armor slot took a saddle");
        require(!window->canHoldInSlot(1, stack("minecraft:iron_nautilus_armor")), "a horse took nautilus armor");

        const std::vector<ItemStack> before = horse.getEquippableItems();
        window->setContainerItem(0, stack("minecraft:saddle"));
        window->setContainerItem(1, stack("minecraft:diamond_horse_armor"));
        require(holds(*window, 1, "minecraft:diamond_horse_armor"), "the armor slot does not show the armor");
        const ItemStack &body = horse.getEquipment().getSlot(MobEquipment::BODY);
        require(body.mDefinition != nullptr && body.mDefinition->getIdentifier() == "minecraft:diamond_horse_armor",
                "the window armor is not worn in the body slot");
        require(horse.getEquipment().getInventoryItem(1).isAir(), "the window armor went into the chest slots");

        horse.onEquippableItemsChanged(arena.mOwner, before, &rider);
        require(horse.getComponent("minecraft:is_saddled") != nullptr, "a window saddle did not saddle the horse");

        MobActor &nautilus = arena.spawn("minecraft:nautilus", "minecraft:spawn_wild_adult");
        require(nautilus.tameTo(arena.mOwner, rider, "minecraft:tameable", false), "the nautilus was not tamed");
        require(nautilus.getInventoryCapacity() == 5, "an inventory without a size is not five slots");
        Container *shell = nautilus.getEntityContainer();
        require(shell != nullptr, "the tamed nautilus has no window");
        require(shell->canHoldInSlot(1, stack("minecraft:copper_nautilus_armor")), "the nautilus refused its armor");
        require(!shell->canHoldInSlot(1, stack("minecraft:copper_horse_armor")), "the nautilus took horse armor");

        MobActor &wolf = arena.spawn("minecraft:wolf", "minecraft:spawn_wild_adult");
        require(wolf.getInventoryCapacity() == 0, "a mob without an inventory got slots");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-mount-armor-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        armorValues();
        Arena arena(directory);
        bodyProtection(arena);
        armorWear(arena);
        wolfArmorInteractions(arena);
        bodySaved(arena);
        mountWindow(arena);
        std::cout << "mount armor checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
