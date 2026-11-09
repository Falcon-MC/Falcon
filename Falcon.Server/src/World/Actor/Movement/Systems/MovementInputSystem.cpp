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

    input.pressingSneak = input.getPacket().hasInputFlag((int32_t) PlayerAuthInputData::Sneaking);

    // the client keeps sprinting only while moving forward: forward > 0, length >= 1/sqrt(2), sideways <= 1/sqrt(2),
    // else it stops on its own; omnisprint is simulated as walking
    const float moveForward = input.getPacket().mMotionY;
    const float moveSideways = input.getPacket().mMotionX;
    const float moveLength = std::sqrt(moveForward * moveForward + moveSideways * moveSideways);
    if (moveForward <= 0.0f || moveLength < SPRINT_INPUT_AXIS || std::fabs(moveSideways) > SPRINT_INPUT_AXIS)
        abilities.sprinting = false;

    // the client already scales its move vector by the item-use slowdown (0.35 squared eating or drawing a bow):
    // don't apply it twice
    float maxImpulse = 1.0f;
    const int32_t slowdownTicks = context.pose.mSneaking || context.pose.mCrawling ? context.player.getSlowdownTicks() + 1 : 0;
    context.player.setSlowdownTicks(slowdownTicks);
    if (!abilities.flying && !abilities.swimming && (context.pose.mSneaking || context.pose.mCrawling)) {
        float sneakMultiplier = SNEAK_INPUT;
        if (slowdownTicks > SWIFT_SNEAK_DELAY_TICKS) {
            const ItemStack &leggings = context.player.getInventory().getArmor(PlayerInventory::ARMOR_LEGS);
            sneakMultiplier += SWIFT_SNEAK_PER_LEVEL
                               * (float) ItemEnchantments::getLevel(leggings, EnchantmentIds::SWIFT_SNEAK);
        }
        maxImpulse *= std::clamp(sneakMultiplier, 0.0f, 1.0f);
    }
    // the wire move vector is already pose-scaled (sneaking sends 0.3): the pose only bounds it, scaling again
    // would slow a sneaking player to 0.09
    const float sideways = std::clamp(input.getPacket().mMotionX, -maxImpulse, maxImpulse);
    const float forward = std::clamp(input.getPacket().mMotionY, -maxImpulse, maxImpulse);
    // 0x140E70890 normalizes the 2D input before damping
    const float length = std::sqrt(sideways * sideways + forward * forward);
    const float scale = IMPULSE_SCALE / std::max(1.0f, length);
    input.sideways = sideways * scale;
    input.forward = forward * scale;
}
