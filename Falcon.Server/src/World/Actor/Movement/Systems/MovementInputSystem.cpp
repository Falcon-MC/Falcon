#include "World/Actor/Movement/Systems/MovementInputSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/EnchantmentData.h"
#include "World/Item/ItemEnchantments.h"

#include <algorithm>
#include <cmath>

using namespace MovementPhysics;

void MovementInputSystem::tick(PlayerMovementContext &context) {
    auto &input = context.input;
    auto &abilities = context.abilities;
    auto &travel = context.travel;

    input.pressingSneak = input.packet.hasInputFlag((int32_t) PlayerAuthInputData::Sneaking);

    float maxImpulse = 1.0f;
    if (context.player.getFlags().get(ActorFlag::UsingItem))
        maxImpulse *= CONSUMING_INPUT;
    const int32_t slowdownTicks = travel.sneaking || travel.crawling ? context.player.getSlowdownTicks() + 1 : 0;
    context.player.setSlowdownTicks(slowdownTicks);
    if (!abilities.flying && !abilities.swimming && (travel.sneaking || travel.crawling)) {
        float sneakMultiplier = SNEAK_INPUT;
        if (slowdownTicks > SWIFT_SNEAK_DELAY_TICKS) {
            const ItemStack &leggings = context.player.getInventory().getArmor(PlayerInventory::ARMOR_LEGS);
            sneakMultiplier += SWIFT_SNEAK_PER_LEVEL
                               * (float) ItemEnchantments::getLevel(leggings, EnchantmentIds::SWIFT_SNEAK);
        }
        maxImpulse *= std::clamp(sneakMultiplier, 0.0f, 1.0f);
    }
    // BDS 0x140E70890 normalizes the raw 2D input before applying pose slowdown and damping.
    const float length = std::sqrt(input.packet.mMotionX * input.packet.mMotionX
                                   + input.packet.mMotionY * input.packet.mMotionY);
    const float scale = maxImpulse * IMPULSE_SCALE / std::max(1.0f, length);
    input.sideways = input.packet.mMotionX * scale;
    input.forward = input.packet.mMotionY * scale;
}
