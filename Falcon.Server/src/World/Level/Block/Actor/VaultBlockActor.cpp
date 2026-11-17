#include "World/Level/Block/Actor/VaultBlockActor.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/ItemStackNbt.h"
#include "World/Level/Level.h"
#include "World/Level/Storage/Loot/LootItems.h"
#include "World/Level/Storage/Loot/LootTableRegistry.h"

#include <algorithm>
#include <utility>

namespace {
    constexpr int32_t INSERT_DELAY = 14;
    constexpr int32_t EJECT_INTERVAL = 20;
    constexpr int32_t DISPLAY_INTERVAL = 20;
    constexpr int32_t ACTIVATE_EVENT = 9811;
    constexpr int32_t DEACTIVATE_EVENT = 9812;
    constexpr int32_t EJECT_EVENT = 9813;
}

Tag VaultBlockActor::saveNbt() const {
    Tag save = Tag::ofCompound();
    save.put("config", mConfig);
    Tag data = Tag::ofCompound();
    if (mDisplay.isCompound())
        data.put("display_item", mDisplay);
    data.put("items_to_eject", Tag::ofList(Tag::Type::Compound, mRewards));
    data.putInt("total_ejections_needed", (int32_t) mRewards.size());
    Tag rewarded = Tag::ofList(Tag::Type::Long);
    for (int64_t id: mRewarded)
        rewarded.addToList(Tag::ofLong(id));
    data.put("rewarded_players", rewarded);
    Tag accounts = Tag::ofList(Tag::Type::String);
    for (const auto &id: mRewardedAccounts)
        accounts.addToList(Tag::ofString(id));
    data.put("FalconRewardedAccounts", accounts);
    data.putInt("FalconDelay", mDelay);
    save.put("data", data);
    return save;
}

Tag VaultBlockActor::getSpawnCompound() const {
    Tag packet = BlockActor::getSpawnCompound();
    packet.put("display_item", mDisplay.isCompound() ? mDisplay : ItemStackNbt::write(ItemStack::air()));
    packet.put("connected_players", mConnected);
    packet.putFloat("connected_particle_range", mConfig.getFloat("deactivation_range", 4.5f));
    return packet;
}

void VaultBlockActor::loadNbt(const Tag &save, const PacketCodecContext &) {
    const Tag *config = save.get("config");
    mConfig = config != nullptr && config->isCompound() ? *config : Tag::ofCompound();
    mDisplay = Tag();
    mConnected = Tag::ofList(Tag::Type::Long);
    mRewards.clear();
    mRewarded.clear();
    mRewardedAccounts.clear();
    mDelay = 0;
    const Tag *data = save.get("data");
    if (data == nullptr || !data->isCompound())
        return;

    if (const Tag *display = data->get("display_item"); display != nullptr && display->isCompound())
        mDisplay = *display;
    if (const Tag *items = data->get("items_to_eject"); items != nullptr && items->isList()) {
        for (const Tag &item: items->getList()) {
            if (item.isCompound())
                mRewards.push_back(item);
        }
    }
    if (const Tag *ids = data->get("rewarded_players"); ids != nullptr && ids->isList()) {
        for (const Tag &entry: ids->getList()) {
            if (entry.getType() == Tag::Type::Long)
                mRewarded.insert(entry.asLong());
        }
    }
    if (const Tag *ids = data->get("FalconRewardedAccounts"); ids != nullptr && ids->isList()) {
        for (const Tag &entry: ids->getList()) {
            if (entry.getType() == Tag::Type::String && !entry.asString().empty())
                mRewardedAccounts.insert(entry.asString());
        }
    }
    mDelay = std::clamp(data->getInt("FalconDelay"), 0, EJECT_INTERVAL);
}

const BlockState *VaultBlockActor::_block() const {
    return mLevel == nullptr ? nullptr : mLevel->peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
}

Vector3f VaultBlockActor::_center() const {
    return Vector3f(mPosition.x + 0.5f, mPosition.y + 0.5f, mPosition.z + 0.5f);
}

bool VaultBlockActor::_eligible(const ServerPlayer &player, float range) const {
    if (!player.isSpawned() || player.isDead() || !player.isIn(*mLevel)
        || player.getGameType() == (int32_t) GameType::Spectator)
        return false;
    const Vector3f center = _center();
    const Vector3f &position = player.getPosition();
    const float x = position.x - center.x;
    const float y = position.y - center.y;
    const float z = position.z - center.z;
    return x * x + y * y + z * z <= range * range;
}

void VaultBlockActor::_setState(const BlockState &block, const char *phase) {
    if (block.mStates.getString("vault_state") == phase)
        return;
    Tag states = block.mStates;
    states.putString("vault_state", phase);
    mLevel->setBlock(mPosition, BlockState(block.mName, states), true);
}

void VaultBlockActor::_setDisplay(ServerNetworkHandler &owner, Tag item) {
    if (item == mDisplay)
        return;
    mDisplay = std::move(item);
    BlockActionHandler::broadcastBlockActorData(owner, *mLevel, *this);
}

std::string VaultBlockActor::_key(const BlockState &block) const {
    const Tag *key = mConfig.get("key_item");
    const std::string name = key == nullptr ? std::string() : key->getString("Name");
    if (!name.empty())
        return name;
    return block.mStates.getByte("ominous") != 0 ? "minecraft:ominous_trial_key" : "minecraft:trial_key";
}

std::vector<Tag> VaultBlockActor::_roll(ServerNetworkHandler &owner, const BlockState &block, bool preview) {
    std::string path = preview ? mConfig.getString("override_loot_table_to_display") : std::string();
    if (path.empty())
        path = mConfig.getString("loot_table");
    if (path.empty() || path == "loot_tables/.json") {
        path = block.mStates.getByte("ominous") != 0 ? "chests/trial_chambers/reward_ominous.json"
                                                     : "chests/trial_chambers/reward.json";
    }
    if (path.starts_with("minecraft:"))
        path.erase(0, sizeof("minecraft:") - 1);
    if (!path.ends_with(".json"))
        path += ".json";
    const LootTable *table = LootTableRegistry::getInstance().get(path);
    if (table == nullptr)
        return {};
    LootContext context(mRandom);
    std::vector<Tag> result;
    for (const LootDrop &drop: table->roll(context)) {
        const ItemStack stack = LootItems::toItemStack(owner, drop);
        if (!stack.isAir() && stack.mCount > 0)
            result.push_back(ItemStackNbt::write(stack));
    }
    return result;
}

bool VaultBlockActor::unlock(ServerNetworkHandler &owner, ServerPlayer &player) {
    const BlockState *block = _block();
    if (block == nullptr || block->mName != "minecraft:vault" || !mRewards.empty())
        return false;

    const std::string phase = block->mStates.getString("vault_state");
    if (phase != "active" || !_eligible(player, mConfig.getFloat("deactivation_range", 4.5f)))
        return false;

    if (mRewarded.count(player.getUniqueId()) != 0 || mRewardedAccounts.count(player.getUuid()) != 0) {
        owner.playLevelSound(*mLevel, "vault.reject_rewarded_player", _center());
        return false;
    }
    const ItemStack held = player.getInventory().getItemInHand();
    const Tag *key = mConfig.get("key_item");
    const int32_t count = key == nullptr ? 1 : std::max<int32_t>(1, key->getByte("Count", 1));
    const int32_t damage = key == nullptr ? 0 : key->getShort("Damage");
    if (held.isAir() || held.mCount < count || held.mDefinition->getIdentifier() != _key(*block)
        || held.mDamage != damage) {
        owner.playLevelSound(*mLevel, "vault.insert_item_fail", _center());
        return false;
    }

    auto rewards = _roll(owner, *block, false);
    if (rewards.empty())
        return false;
    mRewarded.insert(player.getUniqueId());
    if (!player.getUuid().empty())
        mRewardedAccounts.insert(player.getUuid());
    Tag remaining = Tag::ofList(Tag::Type::Long);
    for (const Tag &entry: mConnected.getList()) {
        if (entry.asLong() != player.getUniqueId())
            remaining.addToList(entry);
    }
    mConnected = std::move(remaining);
    mRewards = std::move(rewards);
    for (int32_t i = 0; i < count; ++i)
        player.consumeOneHeldItem();
    mDelay = INSERT_DELAY;
    _setState(*block, "unlocking");
    mDisplay = mRewards.back();
    BlockActionHandler::broadcastBlockActorData(owner, *mLevel, *this);
    owner.playLevelSound(*mLevel, "vault.insert_item", _center());
    return true;
}

bool VaultBlockActor::tick(ServerNetworkHandler &owner) {
    const BlockState *block = _block();
    if (block == nullptr)
        return true;
    if (block->mName != "minecraft:vault")
        return false;

    const std::string phase = block->mStates.getString("vault_state");
    if (!mRewards.empty()) {
        if (mDelay > 0 && --mDelay > 0)
            return true;
        if (phase != "ejecting") {
            _setState(*block, "ejecting");
            owner.playLevelSound(*mLevel, "vault.open_shutter", _center());
            mDelay = EJECT_INTERVAL;
            return true;
        }

        const ItemStack reward = ItemStackNbt::read(mRewards.back(), owner.getCodecContext());
        mRewards.pop_back();
        if (!reward.isAir()) {
            const Vector3f center = _center();
            std::uniform_real_distribution<float> random(0.0f, 1.0f);
            const float vx = (random(mRandom) - random(mRandom)) * 0.034455f;
            const float vz = (random(mRandom) - random(mRandom)) * 0.034455f;
            owner.dropItem(*mLevel, Vector3f(center.x, mPosition.y + 1.075f, center.z), reward,
                           Vector3f(vx, 0.2f, vz), ItemActorHandler::DROP_PICKUP_DELAY);
            owner.playLevelSound(*mLevel, "vault.eject_item", Vector3f(center.x, mPosition.y + 1.2f, center.z));
            owner.broadcastLevelEvent(*mLevel, EJECT_EVENT,
                                      Vector3f((float) mPosition.x, (float) mPosition.y, (float) mPosition.z), 0);
        }
        _setDisplay(owner, mRewards.empty() ? Tag() : mRewards.back());
        mDelay = EJECT_INTERVAL;
        return true;
    }
    if (mDelay > 0 && --mDelay > 0)
        return true;

    const float range = phase == "active" ? mConfig.getFloat("deactivation_range", 4.5f)
                                           : mConfig.getFloat("activation_range", 4.0f);
    Tag connected = Tag::ofList(Tag::Type::Long);
    for (const auto &entry: owner.getPlayers()) {
        const ServerPlayer &player = entry.second;
        if (_eligible(player, range) && mRewarded.count(player.getUniqueId()) == 0
            && mRewardedAccounts.count(player.getUuid()) == 0) {
            connected.addToList(Tag::ofLong(player.getUniqueId()));
        }
    }
    if (connected != mConnected) {
        mConnected = std::move(connected);
        BlockActionHandler::broadcastBlockActorData(owner, *mLevel, *this);
    }
    if (phase == "ejecting" || phase == "unlocking")
        owner.playLevelSound(*mLevel, "vault.close_shutter", _center());
    const Vector3f corner((float) mPosition.x, (float) mPosition.y, (float) mPosition.z);
    if (!mConnected.getList().empty()) {
        if (phase != "active") {
            _setState(*block, "active");
            owner.broadcastLevelEvent(*mLevel, ACTIVATE_EVENT, corner, 0);
        }
        const auto previews = _roll(owner, *block, true);
        if (!previews.empty()) {
            const size_t index = std::uniform_int_distribution<size_t>(0, previews.size() - 1)(mRandom);
            _setDisplay(owner, previews[index]);
        }
    } else if (phase != "inactive") {
        _setState(*block, "inactive");
        _setDisplay(owner, Tag());
        owner.broadcastLevelEvent(*mLevel, DEACTIVATE_EVENT, corner, 0);
    }
    mDelay = DISPLAY_INTERVAL;
    return true;
}
