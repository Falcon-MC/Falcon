#include "World/Level/Block/SignEditing.h"

#include "Core/Color/DyeColor.h"
#include "Protocol/Types/ItemStack.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Packets/OpenSignPacket.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/SignBlockActor.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

namespace {
    const char *TAG_TEXT = "Text";
    const char *TAG_FILTERED = "FilteredText";
    const char *TAG_OWNER = "TextOwner";
    const char *TAG_COLOR = "SignTextColor";
    const char *TAG_GLOWING = "IgnoreLighting";
    const char *TAG_HIDE_OUTLINE = "HideGlowOutline";
    const char *TAG_FRONT = "FrontText";
    const char *TAG_BACK = "BackText";

    // text colors by item color, black first
    const int32_t DYE_TEXT_COLORS[DyeColor::COUNT] = {
            0x000000, 0xB02E26, 0x5E7C16, 0x835432, 0x3C44AA, 0x8932B8, 0x169C9C, 0x9D9D97,
            0x474F52, 0xF38BAA, 0x80C71F, 0xFED83D, 0x3AB3DA, 0xC74EBD, 0xF9801D, 0xF0F0F0,
    };

    std::string itemIdentifier(const ItemStack &item) {
        return item.mDefinition == nullptr ? std::string() : item.mDefinition->getIdentifier();
    }

    bool hasText(const Tag &side) {
        return !side.getString(TAG_TEXT, "").empty();
    }

    Vector3f cornerOf(const Vector3i &position) {
        return Vector3f((float) position.x, (float) position.y, (float) position.z);
    }

    // the item color of a dye item: DyeColor runs white to black, the item colors black to white
    bool dyeItemColor(const std::string &identifier, uint8_t &color) {
        const uint8_t dye = DyeColor::fromIdentifier(identifier, "_dye");
        if (dye == DyeColor::INVALID)
            return false;

        color = (uint8_t) (DyeColor::COUNT - 1 - dye);
        return true;
    }

    void markChanged(ServerNetworkHandler &owner, Level &level, SignBlockActor &sign) {
        BlockActionHandler::broadcastBlockActorData(owner, level, sign);
    }
}

int32_t SignEditing::dyeTextColor(uint8_t itemColor) {
    return (int32_t) (0xFF000000u | (uint32_t) DYE_TEXT_COLORS[std::min<uint8_t>(itemColor, DyeColor::COUNT - 1)]);
}

// from ground_sign_direction when the sign has it, else from
// facing_direction; the hanging sign reads ground_sign_direction only while attached
float SignEditing::rotationOf(const BlockState &state) {
    const bool hanging = state.mName.find("hanging") != std::string::npos;
    const bool attached = state.mStates.getByte("attached_bit", 0) != 0;
    float angle = 180.0f;
    if (state.mStates.get("ground_sign_direction") != nullptr && (!hanging || attached)) {
        angle = (float) (360 * state.mStates.getInt("ground_sign_direction", 0)) * 0.0625f + 180.0f;
    } else {
        static const float FACING_ANGLES[4] = {360.0f, 180.0f, 270.0f, 90.0f};
        const int32_t facing = state.mStates.getInt("facing_direction", 0) - 2;
        if (facing >= 0 && facing <= 3)
            angle = FACING_ANGLES[facing];
    }

    float rotation = std::fmod(angle, 360.0f);
    if (rotation < 0.0f)
        rotation += 360.0f;
    return rotation - 180.0f;
}

// SignBlockActor side test: the side the player stands in front of; the client centers on
// the block shape, the block center here, the same answer anywhere a player can stand
bool SignEditing::isFrontFacing(const BlockState &state, const Vector3i &position, float playerX, float playerZ) {
    const float radians = rotationOf(state) * 0.017453292f;
    const float dx = playerX - ((float) position.x + 0.5f);
    const float dz = playerZ - ((float) position.z + 0.5f);
    return dz * std::cos(radians) - dx * std::sin(radians) >= 0.0f;
}

bool SignEditing::editorStillHolds(ServerNetworkHandler &owner, SignBlockActor &sign) {
    if (sign.getEditor() == -1)
        return false;

    for (auto &entry: owner.getPlayers()) {
        if (entry.second.isSpawned() && entry.second.getUniqueId() == sign.getEditor())
            return true;
    }

    sign.setEditor(-1);
    return false;
}

// one editor at a time, the lock goes to the opening player
bool SignEditing::openEditor(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position, bool front) {
    SignBlockActor *sign = owner.getLevelFor(player).getBlockActors().find<SignBlockActor>(position);
    if (sign == nullptr || editorStillHolds(owner, *sign))
        return false;

    sign->setEditor(player.getUniqueId());
    OpenSignPacket packet;
    packet.mPosition = position;
    packet.mFrontSide = front;
    owner.sendPacketTo(player.getNetworkIdentifier(), packet);
    return true;
}

// SignBlock interaction choice
SignEditing::Action SignEditing::decide(ServerNetworkHandler &owner, SignBlockActor &sign, ServerPlayer &player,
                                        bool front, const ItemStack &held) {
    if (player.isAdventure() || editorStillHolds(owner, sign))
        return Action::None;

    if (sign.isWaxed())
        return Action::WaxedFail;

    Tag &side = sign.getSide(front);
    const std::string identifier = itemIdentifier(held);
    uint8_t itemColor = 0;
    if (identifier == "minecraft:honeycomb")
        return Action::Honeycomb;
    if (identifier == "minecraft:glow_ink_sac") {
        if (!side.getBool(TAG_GLOWING, false) && hasText(side))
            return Action::GlowInkSac;
    } else if (identifier == "minecraft:ink_sac") {
        if (side.getBool(TAG_GLOWING, false) && hasText(side))
            return Action::InkSac;
    } else if (dyeItemColor(identifier, itemColor)) {
        if (side.getInt(TAG_COLOR, (int32_t) 0xFF000000) != dyeTextColor(itemColor) && hasText(side))
            return Action::Dye;
    }

    return Action::Edit;
}

bool SignEditing::interact(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                           const BlockState &state) {
    Level &level = owner.getLevelFor(player);
    SignBlockActor *sign = level.getBlockActors().find<SignBlockActor>(position);
    if (sign == nullptr)
        return false;

    const Vector3f &feet = player.getPosition();
    const bool front = isFrontFacing(state, position, feet.x, feet.z);
    const ItemStack held = player.getInventory().getItemInHand();
    Tag &side = sign->getSide(front);

    switch (decide(owner, *sign, player, front, held)) {
        case Action::None:
            return false;
        case Action::WaxedFail:
            owner.playLevelSound(level, WAXED_FAIL_SOUND, cornerOf(position));
            return true;
        case Action::Dye: {
            uint8_t itemColor = 0;
            dyeItemColor(itemIdentifier(held), itemColor);
            side.putInt(TAG_COLOR, dyeTextColor(itemColor));
            side.putBool(TAG_HIDE_OUTLINE, false);
            player.consumeOneHeldItem();
            owner.broadcastLevelEvent(level, DYE_EVENT, cornerOf(position), 0);
            break;
        }
        case Action::GlowInkSac:
        case Action::InkSac:
            side.putBool(TAG_GLOWING, held.mDefinition->getIdentifier() == "minecraft:glow_ink_sac");
            side.putBool(TAG_HIDE_OUTLINE, false);
            player.consumeOneHeldItem();
            owner.broadcastLevelEvent(level, INK_SAC_EVENT, cornerOf(position), 0);
            break;
        case Action::Honeycomb:
            player.consumeOneHeldItem();
            owner.broadcastLevelEvent(
                    level, WAX_ON_EVENT,
                    Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f), 0);
            sign->setWaxed(true);
            break;
        case Action::Edit:
            openEditor(owner, player, position, front);
            return true;
    }

    markChanged(owner, level, *sign);
    return true;
}

// a sign placed from an item without written sides opens on its front
void SignEditing::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                           const ItemStack &usedItem) {
    if (usedItem.mTag.isCompound()
        && (usedItem.mTag.get(TAG_FRONT) != nullptr || usedItem.mTag.get(TAG_BACK) != nullptr))
        return;

    // placement does not make the sign's block actor: the hanging signs keep their own id
    BlockActorStore &blockActors = owner.getLevelFor(player).getBlockActors();
    if (blockActors.find<SignBlockActor>(position) == nullptr) {
        const BlockState state = owner.getLevelFor(player).getBlockState(position.x, position.y, position.z);
        std::unique_ptr<SignBlockActor> sign(new SignBlockActor(state.mName.find("hanging") != std::string::npos));
        sign->setPosition(position);
        sign->setState(state);
        blockActors.insert(std::move(sign));
    }
    openEditor(owner, player, position, true);
}

// only the text of each side, its owner is the editor;
// the lock is released afterwards
bool SignEditing::applyUpdate(ServerNetworkHandler &owner, ServerPlayer &player, SignBlockActor &sign,
                              const Tag &update) {
    if (!editorStillHolds(owner, sign) || sign.getEditor() != player.getUniqueId() || sign.isWaxed())
        return false;

    static const size_t MAX_SIGN_TEXT = 512;
    const char *const keys[] = {TAG_FRONT, TAG_BACK};
    for (int index = 0; index < 2; ++index) {
        const Tag *incoming = update.get(keys[index]);
        if (incoming == nullptr || !incoming->isCompound())
            continue;

        Tag &side = sign.getSide(index == 0);
        const std::string text = incoming->getString(TAG_TEXT, "").substr(0, MAX_SIGN_TEXT);
        if (text == side.getString(TAG_TEXT, ""))
            continue;

        side.putString(TAG_TEXT, text);
        if (incoming->contains(TAG_FILTERED, Tag::Type::String))
            side.putString(TAG_FILTERED, incoming->getString(TAG_FILTERED).substr(0, MAX_SIGN_TEXT));
        side.putString(TAG_OWNER, player.getXuid());
    }

    sign.setEditor(-1);
    return true;
}
