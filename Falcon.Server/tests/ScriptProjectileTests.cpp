#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Projectile/ProjectileComponent.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void writeFile(const std::filesystem::path &path, const std::string &contents) {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << contents;
    }

    void writePack(const std::filesystem::path &root, const std::string &name, const std::string &version,
                   const std::string &script) {
        writeFile(root / name / "manifest.json",
                  "{\"format_version\": 2, \"header\": {\"name\": \"" + name
                          + "\", \"uuid\": \"00000000-0000-4000-8000-5000000000" + (version[0] == '2' ? "02" : "01")
                          + "\", \"version\": [1, 0, 0], \"min_engine_version\": [1, 21, 0]},"
                            " \"modules\": [{\"type\": \"script\", \"language\": \"javascript\","
                            " \"uuid\": \"00000000-0000-4000-9000-5000000000"
                          + (version[0] == '2' ? "02" : "01")
                          + "\", \"entry\": \"scripts/main.js\", \"version\": [1, 0, 0]}],"
                            " \"dependencies\": [{\"module_name\": \"@minecraft/server\", \"version\": \""
                          + version + "\"}]}\n");
        writeFile(root / name / "scripts" / "main.js", script);
    }

    const char *const SCRIPT = R"JS(
import { world, system } from "@minecraft/server";

const r = {};
const round = (v) => Math.round(v * 1000) / 1000;
const fail = (f) => { try { f(); return "ok"; } catch (e) { return e.name; } };
const read = (type) => {
    const entity = world.getDimension("overworld").getEntities({ type })[0];
    const p = entity.getComponent("minecraft:projectile");
    return [round(p.gravity), round(p.airInertia), round(p.liquidInertia), round(p.onFireTime), p.catchFireOnHurt,
        p.critParticlesOnProjectileHurt, p.destroyOnProjectileHurt, p.lightningStrikeOnHit, p.stopOnHit,
        p.shouldBounceOnHit, String(p.hitEntitySound), String(p.hitGroundSound), String(p.hitParticle)].join(",");
};
system.afterEvents.scriptEventReceive.subscribe((e) => {
    if (e.id !== "projectile:read")
        return;
    r.snowball = read("minecraft:snowball");
    r.trident = read("minecraft:thrown_trident");
    r.bullet = read("minecraft:shulker_bullet");
    const snowball = world.getDimension("overworld").getEntities({ type: "minecraft:snowball" })[0];
    const p = snowball.getComponent("projectile");
    p.gravity = 0;
    p.airInertia = 0.5;
    r.badParticle = fail(() => { p.hitParticle = "none"; }) + ":" + fail(() => { p.hitParticle = "nope"; })
        + ":" + fail(() => { p.hitParticle = "Crit"; });
    p.hitParticle = "crit";
    r.crit = String(p.hitParticle);
    p.hitParticle = undefined;
    r.badSound = fail(() => { p.hitEntitySound = "random.pop"; }) + ":"
        + fail(() => { p.hitEntitySound = "undefined"; });
    p.hitGroundSound = "BOW.HIT";
    r.groundSound = String(p.hitGroundSound);
    p.hitGroundSound = undefined;
    p.hitEntitySound = "bow.hit";
    p.shouldBounceOnHit = true;
    p.lightningStrikeOnHit = true;
    r.written = read("minecraft:snowball");
    r.badType = fail(() => { p.gravity = "x"; }) + ":" + fail(() => { p.stopOnHit = 1; });
    world.setDynamicProperty("projectile", JSON.stringify(r));
});
)JS";

    void expect(const json::Value &values, const char *key, const std::string &wanted) {
        const json::Value *value = values.get(key);
        require(value != nullptr, std::string("missing result ") + key);
        require(value->string() == wanted,
                std::string(key) + ": expected \"" + wanted + "\", got \"" + value->string() + "\"");
    }

    ServerActor &spawn(ServerNetworkHandler &owner, Level &level, const char *identifier, float x) {
        ServerActor *actor = owner.spawnActor(level, identifier, Vector3f(x, 120.0f, 0.5f));
        require(actor != nullptr, std::string("no fixture ") + identifier);
        actor->setProjectile(true);
        return *actor;
    }

    void run(const std::filesystem::path &directory) {
        const std::filesystem::path packs = directory / "packs";
        writePack(packs, "projectile", "2.11.0", SCRIPT);

        ServerNetworkHandler owner("Script projectile", "", 1);
        ScriptEngine &engine = owner.getScriptEngine();
        engine.bindHost(owner);
        BehaviorPackManager manager;
        manager.discover(packs.string());
        CustomContentRegistry::getInstance().load(manager, owner.getItemDefinitions(), owner.getBlockDefinitions());
        require(engine.loadPacks(manager.getPacks()) == 1, "the projectile pack did not start");

        engine.startup();
        EntityDefinitions::initialize();
        World &world = owner.getWorlds().getDefault();
        Level &level = world.getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        ServerActor &snowball = spawn(owner, level, "minecraft:snowball", 0.5f);
        ServerActor &trident = spawn(owner, level, "minecraft:thrown_trident", 2.5f);
        spawn(owner, level, "minecraft:shulker_bullet", 4.5f);
        engine.tick(0);

        // the definitions set the physics: thrown_trident.json gravity 0.1, liquid_inertia 0.99
        require(trident.getProjectileComponent().mGravity == 0.1f
                        && trident.getProjectileComponent().mLiquidInertia == 0.99f,
                "the trident did not read its definition");
        // no ice_ball definition in the data: the class physics stay
        const std::unique_ptr<ServerActor> iceBall = ActorClassRegistry::create(1, "minecraft:ice_ball");
        require(iceBall != nullptr && iceBall->getProjectileComponent().mGravity == 0.03f
                        && iceBall->getProjectileComponent().mInertia == 0.99f,
                "the ice ball lost its class physics");

        engine.emitScriptEvent("projectile:read", "");
        const auto it = owner.getWorldDynamicProperties().find("projectile");
        require(it != owner.getWorldDynamicProperties().end(), "the projectile pack wrote nothing");
        const std::unique_ptr<json::Value> r = json::parse(it->second.mString);
        require(r != nullptr && r->isObject(), "the projectile pack wrote no JSON");
        expect(*r, "snowball", "0.03,0.99,0.6,5,false,false,false,false,false,false,undefined,undefined,iconcrack");
        expect(*r, "trident",
               "0.1,0.99,0.99,5,false,false,false,false,true,true,item.trident.hit,item.trident.hit_ground,iconcrack");
        expect(*r, "bullet", "0.05,0.99,0.6,5,false,true,true,false,false,true,bullet.hit,undefined,iconcrack");
        expect(*r, "written", "0,0.5,0.6,5,false,false,false,true,false,true,bow.hit,undefined,undefined");
        // sounds are LevelSoundEvent numbers: names are lowercased, an unknown one and "undefined" throw
        expect(*r, "badSound", "InvalidArgumentError:InvalidArgumentError");
        expect(*r, "groundSound", "bow.hit");
        expect(*r, "badType", "TypeError:TypeError");
        // none and unknown names are ParticleType 0, which the setter refuses; names are case sensitive
        expect(*r, "badParticle", "InvalidArgumentError:InvalidArgumentError:InvalidArgumentError");
        expect(*r, "crit", "crit");
        require(ProjectileComponent::particleType("iceballbreak") == 105 && ProjectileComponent::particleType("x") == 0
                        && std::string(ProjectileComponent::particleName(72)) == "blueflame",
                "the particle type table does not follow the game");

        // the written gravity and inertia drive the flight: no fall, half the speed each tick
        snowball.setMotion(Vector3f(0.0f, 0.0f, 1.0f));
        const float height = snowball.getPosition().y;
        owner.tickActors();
        require(snowball.getPosition().y == height && snowball.getMotion().z == 0.5f,
                "the flight ignored the script's gravity and inertia");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-script-projectile-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Script projectile regression failed: " << error.what() << '\n';
        result = 1;
    }
    CustomContentRegistry::getInstance().reset();
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
