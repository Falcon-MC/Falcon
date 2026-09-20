#include "Actor/CushionActor.h"

#include "Actor/ActorClassRegistry.h"
#include "Actor/RideSystem.h"
#include "Actor/ServerPlayer.h"
#include "Block/BlockState.h"
#include "Block/BlockSupport.h"
#include "Core/Color/DyeColor.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/BlockStateHasher.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Types/StartGameTypes.h"

#include <cmath>

FALCON_REGISTER_ACTOR(CushionActor, CushionActor::IDENTIFIER);

namespace {
    constexpr int32_t ACTOR_DATA_VARIANT = 2;

    constexpr const char *TAG_COLOR = "Color";

    constexpr const char *ITEM_SUFFIX = "_cushion";

    constexpr const char *WOOL_SUFFIX = "_wool";
}

CushionActor::CushionActor(uint64_t runtimeId, const std::string &identifier)
        : ServerActor(runtimeId, identifier) {
    getFlags().set(ActorFlag::HasGravity, false);
}

bool CushionActor::hasSupportingBlock(ServerNetworkHandler &owner) const {
    Level &level = owner.getLevelFor(*this);
    const Vector3f position = getPosition();
    const int32_t x = (int32_t) std::floor(position.x);
    const int32_t y = (int32_t) std::floor(position.y) - 1;
    const int32_t z = (int32_t) std::floor(position.z);

    return BlockSupport::isSolidOrCauldron(level.getBlockState(x, y, z));
}

void CushionActor::tick(ServerNetworkHandler &owner) {
    if (mExpired)
        return;

    if (getLifetimeTicks() % SUPPORT_CHECK_PERIOD == 0 && !hasSupportingBlock(owner))
        breakCushion(owner, true);
}

void CushionActor::breakCushion(ServerNetworkHandler &owner, bool dropItem) {
    if (mExpired)
        return;

    mExpired = true;

    RideSystem::ejectAll(owner, *this);

    Level &level = owner.getLevelFor(*this);
    const Vector3f position = getPosition();

    owner.playLevelSound(level, "death", position, IDENTIFIER, -1);

    BlockState wool;
    wool.mName = DyeColor::identifier(mColor, WOOL_SUFFIX);

    LevelEventPacket destroy;
    destroy.mEventId = LevelEventPacket::Event::ParticleDestroy;
    destroy.mPosition = Vector3f(position.x, position.y + 0.5f, position.z);
    destroy.mData = BlockStateHasher::hash(wool.mName, wool.mStates);
    BlockActionHandler::broadcastToViewers(owner, level, destroy.mPosition, destroy);

    if (dropItem)
        owner.spawnItemActor(level, DyeColor::identifier(mColor, ITEM_SUFFIX), 1, position);
}

bool CushionActor::onInteract(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (mExpired || hasPassengers())
        return false;

    return RideSystem::mount(owner, player, *this, true);
}

bool CushionActor::onHurt(ServerNetworkHandler &owner, float amount, ServerPlayer *source) {
    (void) amount;

    if (mExpired)
        return true;

    const bool creative = source != nullptr && source->getGameType() == (int32_t) GameType::Creative;
    breakCushion(owner, !creative);
    return true;
}

void CushionActor::fillSpawnMetadata(EntityDataMap &metadata) const {
    EntityDataEntry flags;
    flags.mId = ActorFlags::FLAGS_DATA_ID;
    flags.mFormat = EntityDataFormat::Long;
    flags.mLongValue = getFlags().getLowBits();
    metadata.mEntries.push_back(flags);

    EntityDataEntry flags2;
    flags2.mId = ActorFlags::FLAGS_2_DATA_ID;
    flags2.mFormat = EntityDataFormat::Long;
    flags2.mLongValue = getFlags().getHighBits();
    metadata.mEntries.push_back(flags2);

    EntityDataEntry variant;
    variant.mId = ACTOR_DATA_VARIANT;
    variant.mFormat = EntityDataFormat::Int;
    variant.mIntValue = DyeColor::invert(mColor);
    metadata.mEntries.push_back(variant);
}

Tag CushionActor::saveNbt() const {
    Tag data = ServerActor::saveNbt();
    data.putByte(TAG_COLOR, (int8_t) mColor);
    return data;
}

void CushionActor::loadNbt(const Tag &data) {
    ServerActor::loadNbt(data);
    mColor = DyeColor::clamp((uint8_t) data.getByte(TAG_COLOR, 0));
}
