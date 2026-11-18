#include "World/Level/Block/Actor/BannerBlockActor.h"

#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/ItemStack.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Level.h"

#include <memory>

namespace {
    const char *TAG_BASE = "Base";
    const char *TAG_TYPE = "Type";
    const char *TAG_PATTERNS = "Patterns";
}

void BannerBlockActor::_putFields(Tag &data) const {
    data.putInt(TAG_BASE, mBase);
    data.putInt(TAG_TYPE, mType);
    // an empty pattern list is left out, as the game saves it
    if (!mPatterns.getList().empty())
        data.put(TAG_PATTERNS, mPatterns);
}

Tag BannerBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    _putFields(data);
    return data;
}

Tag BannerBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    _putFields(data);
    return data;
}

void BannerBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;

    mBase = data.getInt(TAG_BASE, 0);
    mType = data.getInt(TAG_TYPE, 0);
    const Tag *patterns = data.get(TAG_PATTERNS);
    mPatterns = patterns != nullptr && patterns->isList() ? *patterns : Tag::ofList(Tag::Type::Compound);
}

void BannerBlockActor::placeFromItem(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                     const BlockState &state, const ItemStack &item) {
    Tag data = Tag::ofCompound();
    if (item.mTag.isCompound()) {
        const Tag *type = item.mTag.get(TAG_TYPE);
        if (type != nullptr && type->getType() == Tag::Type::Int)
            data.putInt(TAG_TYPE, type->asInt());
        const Tag *patterns = item.mTag.get(TAG_PATTERNS);
        if (patterns != nullptr && patterns->isList())
            data.put(TAG_PATTERNS, *patterns);
    }
    data.putInt(TAG_BASE, item.mDamage);

    BlockActorStore &blockActors = owner.getLevelFor(player).getBlockActors();
    BannerBlockActor *banner = blockActors.find<BannerBlockActor>(position);
    if (banner == nullptr) {
        std::unique_ptr<BannerBlockActor> created(new BannerBlockActor());
        created->setPosition(position);
        created->setState(state);
        banner = created.get();
        blockActors.insert(std::move(created));
    }
    banner->loadNbt(data, owner.getCodecContext());
}
