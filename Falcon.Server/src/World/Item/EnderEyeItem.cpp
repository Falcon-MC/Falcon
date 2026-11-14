#include "World/Item/EnderEyeItem.h"

#include "World/Actor/Misc/EyeOfEnderActor.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/ItemClassRegistry.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/OverworldGeneratorConstants.h"
#include "World/Level/LevelGen/Structure/StructureFeature.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

#include <cmath>
#include <random>

namespace {
    const char *const ENDER_EYE_IDENTIFIER = "minecraft:ender_eye";
    // the search reports the stronghold this far under the sea
    const int32_t TARGET_DEPTH = 31;
    // the eye leaves half a block off the player's eyes on every axis
    const float SPAWN_OFFSET = 0.5f;

    std::mt19937 &survivalRandom() {
        static std::mt19937 random(std::random_device{}());
        return random;
    }
}

FALCON_REGISTER_ITEM(EnderEyeItem, 100);

EnderEyeItem::EnderEyeItem(const Item &base) : Item(base) {}

bool EnderEyeItem::matches(const std::string &identifier) {
    return identifier == ENDER_EYE_IDENTIFIER;
}

bool EnderEyeItem::findTarget(const ChunkGenerator &generator, const Vector3f &position, Vector3i &out) {
    const StructureFeature *strongholds = StructureFeature::get("stronghold");
    if (strongholds == nullptr || generator.getDimensionType() != DimensionType::Overworld)
        return false;

    StructureChunkPos start;
    if (!strongholds->findNearest(generator, (int32_t) std::floor(position.x), (int32_t) std::floor(position.z),
                                  start))
        return false;

    const int32_t offset = strongholds->getBlockOffset();
    out = Vector3i(start.x * 16 + offset, OverworldGeneratorConstants::SEA_LEVEL - TARGET_DEPTH, start.z * 16 + offset);
    return true;
}

bool EnderEyeItem::onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const {
    (void) item;
    Level &level = owner.getLevelFor(player);
    if (level.shareGenerator() == nullptr)
        return false;

    // the game's player position is at its eyes
    Vector3f eyes = player.getPosition();
    eyes.y += MovementPhysics::PLAYER_EYE_HEIGHT;
    Vector3i target;
    if (!findTarget(level.getGenerator(), eyes, target))
        return false;

    owner.playLevelSound(level, LevelSoundEvent::THROW, player.getPosition(), "minecraft:player");
    const Vector3f spawn(eyes.x + SPAWN_OFFSET, eyes.y + SPAWN_OFFSET, eyes.z + SPAWN_OFFSET);
    ServerActor *eye = owner.spawnActor(level, EyeOfEnderActor::IDENTIFIER, spawn, [&target](ServerActor &actor) {
        if (EyeOfEnderActor *signal = dynamic_cast<EyeOfEnderActor *>(&actor))
            signal->signalTo(target, survivalRandom());
    });
    if (eye == nullptr)
        return false;

    player.consumeOneHeldItem();
    return true;
}
