#include "World/Actor/Projectile/ProjectileReflection.h"

#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Definition/Molang.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Level.h"

#include <cmath>
#include <string>

namespace {
    const char *const COMPONENT = "minecraft:reflect_projectiles";
    const char *const NAMESPACE = "minecraft:";
    const char *const DEFAULT_SOUND = "reflect";
    const float DEGREES_TO_RADIANS = 0.017453292f;

    // a number or a Molang expression, worked out against the reflecting entity
    float valueOf(const json::Value &component, const char *key, float fallback, const ServerActor &actor) {
        const json::Value *value = component.get(key);
        if (value == nullptr)
            return fallback;
        if (value->isString())
            return (float) Molang::evaluate(value->string(), actor).mNumber;
        return (float) value->number(fallback);
    }

    bool listed(const json::Value &component, const std::string &identifier) {
        const json::Value *names = component.get("reflected_projectiles");
        if (names == nullptr || !names->isArray())
            return false;
        const std::string bare = identifier.rfind(NAMESPACE, 0) == 0 ? identifier.substr(std::string(NAMESPACE).size())
                                                                     : identifier;
        for (const std::unique_ptr<json::Value> &name: names->mArray) {
            const std::string &entry = name->string();
            if (entry == bare || entry == identifier)
                return true;
        }
        return false;
    }
}

Vector3f ProjectileReflection::turned(const Vector3f &velocity, float azimuthDegrees, float elevationDegrees,
                                      float scale) {
    const float speed = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
    if (speed < 0.0000001f)
        return Vector3f();

    const float yaw = std::atan2(velocity.z, velocity.x) + azimuthDegrees * DEGREES_TO_RADIANS;
    const float pitch = std::asin(std::max(-1.0f, std::min(1.0f, velocity.y / speed)))
                        + elevationDegrees * DEGREES_TO_RADIANS;
    const float kept = speed * scale;
    const float horizontal = std::cos(pitch) * kept;
    return Vector3f(std::cos(yaw) * horizontal, std::sin(pitch) * kept, std::sin(yaw) * horizontal);
}

bool ProjectileReflection::reflect(ServerNetworkHandler &owner, Level &level, ServerActor &projectile,
                                   ServerActor &target) {
    const MobActor *mob = dynamic_cast<const MobActor *>(&target);
    const json::Value *component = mob == nullptr ? nullptr : mob->getComponent(COMPONENT);
    if (component == nullptr || !listed(*component, projectile.getIdentifier()))
        return false;

    const float azimuth = valueOf(*component, "azimuth_angle", 0.0f, target);
    const float elevation = valueOf(*component, "elevation_angle", 0.0f, target);
    const float scale = valueOf(*component, "reflection_scale", 1.0f, target);
    projectile.setMotion(turned(projectile.getMotion(), azimuth, elevation, scale));

    const json::Value *sound = component->get("reflection_sound");
    owner.playLevelSound(level, sound != nullptr && !sound->string().empty() ? sound->string() : DEFAULT_SOUND,
                         projectile.getPosition());
    return true;
}
