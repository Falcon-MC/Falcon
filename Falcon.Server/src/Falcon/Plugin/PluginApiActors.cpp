#include "Falcon/Plugin/PluginServerApi.h"

#include "Falcon/Plugin/PluginApiHelpers.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/CameraInstructionPacket.h"
#include "Protocol/Types/CameraPresets.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Mob/MobEquipment.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/RideSystem.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Level.h"

#include <cmath>
#include <string>

using namespace PluginApiHelpers;

namespace {
    constexpr FalconCameraEase LAST_EASE = FALCON_CAMERA_EASE_IN_OUT_BOUNCE;

    MobActor *mobOf(FalconEntity *target) {
        return dynamic_cast<MobActor *>(entity(target));
    }

    FalconItem *entityEquipment(FalconEntity *target, FalconEquipmentSlot slot) {
        const MobActor *mob = mobOf(target);
        if (mob == nullptr || slot >= (uint32_t) MobEquipment::SLOT_COUNT)
            return nullptr;
        return copyOf(mob->getEquipment().getSlot((int) slot));
    }

    int entitySetEquipment(FalconEntity *target, FalconEquipmentSlot slot, FalconItem *stack) {
        MobActor *mob = mobOf(target);
        if (mob == nullptr || slot >= (uint32_t) MobEquipment::SLOT_COUNT)
            return 0;
        mob->getEquipment().setSlot((int) slot, valueOf(stack));
        mob->getEquipment().broadcast(owner(), *mob);
        return 1;
    }

    FalconEntity *entityVehicle(FalconEntity *target) {
        const Actor *value = entity(target);
        return value->isRiding() ? toHandle(RideSystem::resolve(owner(), value->getVehicleId())) : nullptr;
    }

    uint32_t entityPassengerCount(FalconEntity *target) {
        return (uint32_t) entity(target)->getPassengers().size();
    }

    FalconEntity *entityPassenger(FalconEntity *target, uint32_t index) {
        const std::vector<int64_t> &passengers = entity(target)->getPassengers();
        return index < passengers.size() ? toHandle(RideSystem::resolve(owner(), passengers[index])) : nullptr;
    }

    int entityMount(FalconEntity *rider, FalconEntity *vehicle) {
        auto *ridden = vehicle == nullptr ? nullptr : dynamic_cast<ServerActor *>(entity(vehicle));
        if (ridden == nullptr || entity(rider) == ridden)
            return 0;
        return RideSystem::mount(owner(), *entity(rider), *ridden, false) ? 1 : 0;
    }

    void entityDismount(FalconEntity *rider) {
        Actor *value = entity(rider);
        if (value->isRiding())
            RideSystem::dismount(owner(), *value, false);
    }

    void entityEjectPassengers(FalconEntity *vehicle) {
        RideSystem::ejectAll(owner(), *entity(vehicle));
    }

    uint32_t entityTagCount(FalconEntity *target) {
        return (uint32_t) entity(target)->getTags().size();
    }

    const char *entityTag(FalconEntity *target, uint32_t index) {
        const std::vector<std::string> &tags = entity(target)->getTags();
        return index < tags.size() ? hold(tags[index]) : nullptr;
    }

    int entityHasTag(FalconEntity *target, const char *tag) {
        return tag != nullptr && entity(target)->hasTag(tag) ? 1 : 0;
    }

    int entityAddTag(FalconEntity *target, const char *tag) {
        return tag != nullptr && *tag != '\0' && entity(target)->addTag(tag) ? 1 : 0;
    }

    int entityRemoveTag(FalconEntity *target, const char *tag) {
        return tag != nullptr && entity(target)->removeTag(tag) ? 1 : 0;
    }

    FalconEntity *entityAttackTarget(FalconEntity *target) {
        const MobActor *mob = mobOf(target);
        return mob == nullptr ? nullptr : toHandle(mob->getTarget(owner()));
    }

    int entitySetAttackTarget(FalconEntity *target, FalconEntity *attacked) {
        MobActor *mob = mobOf(target);
        if (mob == nullptr)
            return 0;
        if (attacked == nullptr) {
            mob->clearTarget();
            return 1;
        }

        const Actor *victim = entity(attacked);
        if (victim == mob || !mob->canTarget(*victim))
            return 0;
        return mob->setTarget(owner(), victim->getRuntimeId()) ? 1 : 0;
    }

    int playerSpawnPoint(FalconPlayer *target, FalconBlockPos *position, FalconDimension *dimension) {
        const ServerPlayer *value = player(target);
        if (!value->hasSpawnPoint())
            return 0;
        const Vector3i &spawn = value->getSpawnPoint();
        if (position != nullptr)
            *position = FalconBlockPos{spawn.x, spawn.y, spawn.z};
        if (dimension != nullptr)
            *dimension = (FalconDimension) value->getSpawnDimension();
        return 1;
    }

    // like /spawnpoint: the position and dimension, no bed or anchor holding it
    int playerSetSpawnPoint(FalconPlayer *target, FalconLevel *handle, FalconBlockPos position) {
        if (handle == nullptr)
            return 0;
        const Level &destination = *level(handle);
        if (position.y < destination.getMinY() || position.y > destination.getMaxY())
            return 0;
        player(target)->setSpawnPosition(Vector3i(position.x, position.y, position.z),
                                         destination.getDimensionType(), destination.getName());
        return 1;
    }

    void playerClearSpawnPoint(FalconPlayer *target) {
        player(target)->clearSpawnPoint();
    }

    const char *playerLanguage(FalconPlayer *target) {
        return hold(player(target)->getLocale());
    }

    int32_t playerDeviceOs(FalconPlayer *target) {
        return player(target)->getBuildPlatform();
    }

    const char *playerSkinId(FalconPlayer *target) {
        return hold(player(target)->getSkin().mSkinId);
    }

    const uint8_t *playerSkinImage(FalconPlayer *target, uint32_t *width, uint32_t *height, uint32_t *length) {
        const SkinImageData &image = player(target)->getSkin().mSkinData;
        if (width != nullptr)
            *width = (uint32_t) image.mWidth;
        if (height != nullptr)
            *height = (uint32_t) image.mHeight;
        if (length != nullptr)
            *length = (uint32_t) image.mData.size();
        return image.mData.empty() ? nullptr : reinterpret_cast<const uint8_t *>(hold(image.mData));
    }

    void sendCamera(ServerPlayer &target, const CameraInstructionPacket &packet) {
        owner().getNetworkHandler().send(target.getNetworkIdentifier(), packet, owner().getCodecContext());
        target.recordCamera(packet);
    }

    bool finiteVec3(const FalconVec3 *value) {
        return value == nullptr || (std::isfinite(value->x) && std::isfinite(value->y) && std::isfinite(value->z));
    }

    int playerSetCamera(FalconPlayer *target, const char *preset, const FalconVec3 *position,
                        const FalconVec3 *rotation, float easeTime, FalconCameraEase ease) {
        if (preset == nullptr || ease > LAST_EASE || !std::isfinite(easeTime) || !finiteVec3(position)
            || !finiteVec3(rotation))
            return 0;
        const int presetId = cameraPresetRuntimeId(preset);
        if (presetId < 0)
            return 0;

        CameraSetInstruction set;
        set.mPresetRuntimeId = presetId;
        if (easeTime > 0.0f) {
            set.mEase.mHasValue = true;
            set.mEase.mTime = easeTime;
            set.mEase.mEaseType = (CameraEase) ease;
        }
        if (position != nullptr) {
            set.mHasPos = true;
            set.mPos = Vector3f((float) position->x, (float) position->y, (float) position->z);
        }
        if (rotation != nullptr) {
            set.mHasRot = true;
            set.mRot = Vector2f((float) rotation->x, (float) rotation->y);
        }

        CameraInstructionPacket packet;
        packet.mHasSetInstruction = true;
        packet.mSetInstruction = set;
        sendCamera(*player(target), packet);
        return 1;
    }

    int playerFadeCamera(FalconPlayer *target, float fadeIn, float hold, float fadeOut, uint32_t color) {
        if (!(fadeIn >= 0.0f && hold >= 0.0f && fadeOut >= 0.0f) || !std::isfinite(fadeIn + hold + fadeOut))
            return 0;

        CameraFadeInstruction fade;
        fade.mHasTimeData = true;
        fade.mTimeData.mFadeInTime = fadeIn;
        fade.mTimeData.mWaitTime = hold;
        fade.mTimeData.mFadeOutTime = fadeOut;
        fade.mHasColor = true;
        fade.mColor.mRed = (float) ((color >> 16) & 0xff) / 255.0f;
        fade.mColor.mGreen = (float) ((color >> 8) & 0xff) / 255.0f;
        fade.mColor.mBlue = (float) (color & 0xff) / 255.0f;

        CameraInstructionPacket packet;
        packet.mHasFadeInstruction = true;
        packet.mFadeInstruction = fade;
        sendCamera(*player(target), packet);
        return 1;
    }

    void playerClearCamera(FalconPlayer *target) {
        CameraInstructionPacket packet;
        packet.mHasClear = true;
        packet.mClear = true;
        sendCamera(*player(target), packet);
    }

    int32_t playerItemCooldown(FalconPlayer *target, FalconItem *stack) {
        if (stack == nullptr)
            return 0;
        return player(target)->getItemCooldownRemaining(*item(stack), owner().getCurrentTick());
    }

    int playerStartItemCooldown(FalconPlayer *target, FalconItem *stack, int32_t ticks) {
        if (stack == nullptr || ticks <= 0 || item(stack)->isAir())
            return 0;
        player(target)->startItemCooldown(*item(stack), owner().getCurrentTick(), ticks);
        return 1;
    }
}

void PluginServerApi::fillActors(FalconServerApi &api) {
    api.entityEquipment = &entityEquipment;
    api.entitySetEquipment = &entitySetEquipment;
    api.entityVehicle = &entityVehicle;
    api.entityPassengerCount = &entityPassengerCount;
    api.entityPassenger = &entityPassenger;
    api.entityMount = &entityMount;
    api.entityDismount = &entityDismount;
    api.entityEjectPassengers = &entityEjectPassengers;
    api.entityTagCount = &entityTagCount;
    api.entityTag = &entityTag;
    api.entityHasTag = &entityHasTag;
    api.entityAddTag = &entityAddTag;
    api.entityRemoveTag = &entityRemoveTag;
    api.entityAttackTarget = &entityAttackTarget;
    api.entitySetAttackTarget = &entitySetAttackTarget;
    api.playerSpawnPoint = &playerSpawnPoint;
    api.playerSetSpawnPoint = &playerSetSpawnPoint;
    api.playerClearSpawnPoint = &playerClearSpawnPoint;
    api.playerLanguage = &playerLanguage;
    api.playerDeviceOs = &playerDeviceOs;
    api.playerSkinId = &playerSkinId;
    api.playerSkinImage = &playerSkinImage;
    api.playerSetCamera = &playerSetCamera;
    api.playerFadeCamera = &playerFadeCamera;
    api.playerClearCamera = &playerClearCamera;
    api.playerItemCooldown = &playerItemCooldown;
    api.playerStartItemCooldown = &playerStartItemCooldown;
}
