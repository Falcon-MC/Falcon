#include "World/Actor/Movement/Systems/CurrentSwimAmountSystem.h"
#include "World/Actor/Movement/PlayerMovementContext.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <algorithm>

void CurrentSwimAmountSystem::tick(PlayerMovementContext &context) {
    auto &amount = context.player.getSwimAmount();
    amount.mPrevious = amount.mCurrent;
    // BDS CurrentSwimAmountSystem (0x140BEE130), after this input's movement.
    const bool horizontal = context.abilities.swimming || context.travel.crawling;
    amount.mCurrent = horizontal ? std::min(amount.mCurrent + 0.1f, 1.0f)
                                 : std::max(amount.mCurrent - 0.1f, 0.0f);
}
