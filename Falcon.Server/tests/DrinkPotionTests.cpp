#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/DrinkPotionGoal.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/MobEffect.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

// a test server does not listen, so its tick returns at once: the clock is moved by hand
class ServerTickProbe {
public:
    static void advance(ServerNetworkHandler &owner, int64_t ticks) {
        owner.mCurrentTick += ticks;
    }
};

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void run(const std::filesystem::path &directory) {
        ServerNetworkHandler owner("Drink potion", "", 1);
        const std::filesystem::path properties = directory / "drink.properties";
        std::ofstream(properties) << "difficulty=normal\n";
        owner.setProperties(PropertiesSettings(properties.string()));
        EntityDefinitions::initialize();
        Level &level = owner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
        level.insertChunk(LevelChunk(0, 0)).setPopulated(true);
        MobActor *witch =
                dynamic_cast<MobActor *>(owner.spawnActor(level, "minecraft:witch", Vector3f(4.5f, 100.0f, 4.5f)));
        require(witch != nullptr, "no witch");

        // swiftness (14), always picked: no filters, a chance of 1
        DrinkPotionGoal::Potion swiftness;
        swiftness.mId = 14;
        swiftness.mChance = 1.0f;
        DrinkPotionGoal goal({swiftness});
        for (int call = 0; call < 5; ++call)
            require(!goal.canUse(owner, *witch), "the goal tried before its sixth call");
        require(goal.canUse(owner, *witch), "a certain potion was not picked on the sixth call");
        const ItemStack &held = witch->getEquipment().getSlot(MobEquipment::MAINHAND);
        require(!held.isAir() && held.mDefinition->getIdentifier() == "minecraft:potion" && held.mDamage == 14,
                "the witch does not hold the swiftness potion");
        require(witch->getFlags().get(ActorFlag::UsingItem) && goal.canContinueToUse(owner, *witch),
                "the witch is not drinking");

        goal.tick(owner, *witch);
        require(!witch->hasEffect(MobEffectId::Speed), "the potion worked before it was drunk");
        for (int tick = 0; tick < 40 && witch->getFlags().get(ActorFlag::UsingItem); ++tick) {
            ServerTickProbe::advance(owner, 1);
            goal.tick(owner, *witch);
        }
        require(witch->hasEffect(MobEffectId::Speed) && !goal.canContinueToUse(owner, *witch),
                "the drink did not end with its effect");
        goal.stop(owner, *witch);
        require(witch->getEquipment().getSlot(MobEquipment::MAINHAND).isAir(), "the bottle stayed in the hand");

        // a potion whose filters fail is not drunk
        DrinkPotionGoal::Potion never = swiftness;
        never.mFilters = std::shared_ptr<json::Value>(
                json::parse(R"({"test": "is_underwater", "subject": "self", "value": true})"));
        DrinkPotionGoal dry({never});
        bool drank = false;
        for (int call = 0; call < 12; ++call)
            drank = drank || dry.canUse(owner, *witch);
        require(!drank, "a potion was drunk though its filters failed");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-drink-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
        std::cout << "drink potion checks passed\n";
    } catch (const std::exception &error) {
        std::clog << "Drink potion regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
