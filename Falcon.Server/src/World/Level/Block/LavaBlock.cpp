#include "World/Level/Block/LavaBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/LiquidProperties.h"
#include "World/Level/Block/Systems/FireSystem.h"
#include "World/Level/Block/Systems/LiquidPhysicsSystem.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/Block/BlockNames.h"

FALCON_REGISTER_BLOCK(LavaBlock, 325);

bool LavaBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::LAVA || identifier == "minecraft:flowing_lava";
}

bool LavaBlock::isDynamic(const std::string &identifier) {
    return identifier == "minecraft:flowing_lava";
}

int32_t LavaBlock::getTickDelay(DimensionType dimension) const {
    return dimension == DimensionType::Nether ? 10 : 30;
}

// LiquidBlock on BlockRandomTickEvent sets fire too
void LavaBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                             const BlockState &state) const {
    (void) owner;
    (void) state;
    spreadFire(level.getLiquidPhysics(), position);
}

void LavaBlock::spreadFire(LiquidPhysicsSystem &ticks, const Vector3i &position) {
    static const BlockState fire("minecraft:fire", [] {
        Tag states = Tag::ofCompound();
        states.putInt("age", 0);
        return states;
    }());
    static const int32_t sides[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    if (!ticks.getLevel().getGameRules().getBool("dofiretick"))
        return;
    if (LiquidProperties::of(ticks.stateAt(position)).material != LiquidMaterial::Lava)
        return;

    const BlockState &above = ticks.stateAt(Vector3i(position.x, position.y + 1, position.z));
    if (!LiquidProperties::of(above).air && FireSystem::getBurnChance(above.mName) <= 0)
        return;

    for (int attempt = 0; attempt < 10; ++attempt) {
        const int32_t x = RandomTickSystem::nextInt(3) - 1;
        const int32_t y = RandomTickSystem::nextInt(3) - 1;
        const int32_t z = RandomTickSystem::nextInt(3) - 1;
        const Vector3i candidate(position.x + x, position.y + y, position.z + z);
        if (!ticks.isLoaded(candidate) || FireSystem::getBurnChance(ticks.stateAt(candidate).mName) <= 0)
            continue;

        for (const auto &side: sides) {
            const Vector3i target(candidate.x + side[0], candidate.y + side[1], candidate.z + side[2]);
            if (!LiquidProperties::of(ticks.stateAt(target)).air)
                continue;
            const Block *block = VanillaBlocks::fromIdentifier(fire.mName);
            const BlockState placed = block != nullptr ? block->updateShape(ticks.getLevel(), target, fire) : fire;
            if (ticks.allows(target, placed, BlockChangeCause::Spread, &position))
                ticks.write(target, 0, placed);
            return;
        }
    }
}
