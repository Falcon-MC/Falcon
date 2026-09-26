#include "Block/Blocks/CopperGolemStatueBlock.h"

#include "Actor/Mob/MobActor.h"
#include "Actor/Mob/Passive/CopperGolemActor.h"
#include "Actor/ServerPlayer.h"
#include "Block/Actor/CopperGolemStatueBlockActor.h"
#include "Block/BlockActorStore.h"
#include "Block/BlockClassRegistry.h"
#include "Block/BlockIdentifier.h"
#include "Inventory/PlayerInventory.h"
#include "Item/Item.h"
#include "Item/VanillaItems.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/BlockActorDataPacket.h"
#include "Protocol/Types/ItemDefinition.h"

#include <random>

FALCON_REGISTER_BLOCK(CopperGolemStatueBlock, 187);

namespace {
    const char *const STATUE_SUFFIX = "copper_golem_statue";
    const char *const UNOXIDIZED_STATUE = "minecraft:copper_golem_statue";
    const char *const SERIALIZE_EVENT = "minecraft:serialize_entity";
    const char *const SERIALIZE_SUCCEEDED_EVENT = "minecraft:serialize_entity_succeeded";
    const char *const RANDOMIZE_POSE_EVENT = "minecraft:randomize_pose";
    const char *const FROM_SERIALIZED_EVENT = "minecraft:from_serialized_entity";
    const char *const TAG_IDENTIFIER = "identifier";
    const char *const TAG_HEALTH = "Health";

    std::mt19937 &poseRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    void broadcastStatue(ServerNetworkHandler &owner, Level &level, const CopperGolemStatueBlockActor &statue) {
        const Vector3i &position = statue.getPosition();

        BlockActorDataPacket data;
        data.mBlockPosition = position;
        data.mData = statue.getSpawnCompound();

        const Vector3f centre((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
        BlockActionHandler::broadcastToViewers(owner, level, centre, data);
    }

    bool isAxe(const ItemStack &item) {
        if (item.isAir())
            return false;

        const Item *type = VanillaItems::fromIdentifier(item.mDefinition->getIdentifier());
        return type != nullptr && type->getToolType() == ToolType::Axe;
    }

    void restoreGolem(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
        const CopperGolemStatueBlockActor *statue = level.getBlockActors().find<CopperGolemStatueBlockActor>(position);
        const Tag stored = statue != nullptr && statue->hasStoredActor() ? statue->getStoredActor() : Tag();
        const bool restored = stored.isCompound();
        const std::string identifier = restored ? stored.getString(TAG_IDENTIFIER, CopperGolemActor::IDENTIFIER)
                                                : std::string(CopperGolemActor::IDENTIFIER);

        level.getBlockActors().remove(position);
        level.setBlock(position, BlockState(), true);

        const Vector3f spawnPosition((float) position.x + 0.5f, (float) position.y, (float) position.z + 0.5f);
        const auto configure = [&owner, &stored, restored, &spawnPosition](ServerActor &spawned) {
            if (!restored)
                return;

            spawned.loadNbt(stored);
            spawned.setPosition(spawnPosition);
            MobActor *mob = dynamic_cast<MobActor *>(&spawned);
            if (mob != nullptr)
                mob->getEquipment().loadNbt(stored, owner.getCodecContext());
        };

        MobActor *mob = dynamic_cast<MobActor *>(owner.spawnActor(level, identifier, spawnPosition, configure));
        if (!restored || mob == nullptr)
            return;

        mob->setHealth(stored.getFloat(TAG_HEALTH, mob->getHealth()));
        owner.syncActorAttributes(*mob);
        mob->fireEvent(owner, FROM_SERIALIZED_EVENT);
    }
}

bool CopperGolemStatueBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, STATUE_SUFFIX);
}

bool CopperGolemStatueBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                        const BlockState &state) const {
    Level &level = owner.getLevelFor(player);
    const ItemStack &held = player.getInventory().getItemInHand();
    if (held.isAir()) {
        CopperGolemStatueBlockActor &statue =
                level.getBlockActors().getOrCreate<CopperGolemStatueBlockActor>(position);
        statue.setPose((statue.getPose() + 1) % CopperGolemStatueBlockActor::POSE_COUNT);
        broadcastStatue(owner, level, statue);
        return true;
    }

    if (state.mName != UNOXIDIZED_STATUE || !isAxe(held))
        return false;

    restoreGolem(owner, level, position);
    return true;
}

void CopperGolemStatueBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                      const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    (void) state;
    (void) usedItem;
    (void) blockFace;

    BlockActorStore &blockActors = owner.getLevelFor(player).getBlockActors();
    blockActors.remove(position);
    blockActors.getOrCreate<CopperGolemStatueBlockActor>(position);
}

void CopperGolemStatueBlock::onBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &state) const {
    (void) owner;
    (void) state;

    level.getBlockActors().remove(position);
}

bool CopperGolemStatueBlock::onActorEvent(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                          const BlockState &state, const std::string &event,
                                          MobActor &source) const {
    (void) state;

    if (event == SERIALIZE_EVENT) {
        level.getBlockActors().getOrCreate<CopperGolemStatueBlockActor>(position).setStoredActor(source.saveNbt());
        source.fireEvent(owner, SERIALIZE_SUCCEEDED_EVENT);
        return true;
    }

    if (event != RANDOMIZE_POSE_EVENT)
        return false;

    CopperGolemStatueBlockActor &statue = level.getBlockActors().getOrCreate<CopperGolemStatueBlockActor>(position);
    std::uniform_int_distribution<int32_t> poses(0, CopperGolemStatueBlockActor::POSE_COUNT - 1);
    statue.setPose(poses(poseRandom()));
    broadcastStatue(owner, level, statue);
    return true;
}

PistonMoveReaction CopperGolemStatueBlock::getPistonMoveReaction() const {
    return PistonMoveReaction::Break;
}
