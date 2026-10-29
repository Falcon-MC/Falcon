#include "World/Level/LevelGen/Overworld/Density/OverworldCavesDensity.h"

#include <utility>

DensityFunctionPtr OverworldCavesDensity::spaghettiRoughnessFunction(
        const NormalNoisePtr &spaghettiRoughness,
        const NormalNoisePtr &spaghettiRoughnessModulator) {
    DensityFunctionPtr roughness = DensityCommon::noise(spaghettiRoughness, 1.0, 1.0);
    DensityFunctionPtr modulator = DensityCommon::mappedNoise(spaghettiRoughnessModulator, 0.0, -0.1);
    return DensityCommon::cacheOnce(
            DensityCommon::mul(
                    std::move(modulator),
                    DensityCommon::add(DensityCommon::abs(std::move(roughness)), DensityCommon::constant(-0.4))
            )
    );
}

DensityFunctionPtr OverworldCavesDensity::spaghetti2dThicknessModulator(const NormalNoisePtr &spaghetti2dThickness) {
    return DensityCommon::cacheOnce(
            DensityCommon::mappedNoise(spaghetti2dThickness, 2.0, 1.0, -0.6, -1.3)
    );
}

DensityFunctionPtr OverworldCavesDensity::entrances(const DensityFunctionPtr &spaghettiRoughnessFunction,
                                                    const NormalNoisePtr &spaghetti3dRarity,
                                                    const NormalNoisePtr &spaghetti3dThickness,
                                                    const NormalNoisePtr &spaghetti3dFirst,
                                                    const NormalNoisePtr &spaghetti3dSecond,
                                                    const NormalNoisePtr &caveEntrance) {
    DensityFunctionPtr rarity = DensityCommon::cacheOnce(DensityCommon::noise(spaghetti3dRarity, 2.0, 1.0));
    DensityFunctionPtr thickness = DensityCommon::mappedNoise(spaghetti3dThickness, -0.065, -0.088);
    DensityFunctionPtr first = DensityCommon::weirdScaledSampler(rarity, spaghetti3dFirst, RarityValueMapper::TYPE1);
    DensityFunctionPtr second = DensityCommon::weirdScaledSampler(rarity, spaghetti3dSecond, RarityValueMapper::TYPE1);
    DensityFunctionPtr spaghetti = DensityCommon::clamp(
            DensityCommon::add(DensityCommon::max(std::move(first), std::move(second)), std::move(thickness)),
            -1.0, 1.0);
    DensityFunctionPtr entrance = DensityCommon::noise(caveEntrance, 0.75, 0.5);
    DensityFunctionPtr entranceGradient = DensityCommon::add(
            DensityCommon::add(std::move(entrance), DensityCommon::constant(0.37)),
            DensityCommon::yClampedGradient(-10, 30, 0.3, 0.0)
    );
    return DensityCommon::cacheOnce(
            DensityCommon::min(
                    std::move(entranceGradient),
                    DensityCommon::add(spaghettiRoughnessFunction, std::move(spaghetti))
            )
    );
}

DensityFunctionPtr OverworldCavesDensity::pillars(const NormalNoisePtr &pillar, const NormalNoisePtr &pillarRareness,
                                                  const NormalNoisePtr &pillarThickness) {
    DensityFunctionPtr pillarNoise = DensityCommon::noise(pillar, 25.0, 0.3);
    DensityFunctionPtr rarity = DensityCommon::mappedNoise(pillarRareness, 0.0, -2.0);
    DensityFunctionPtr thickness = DensityCommon::mappedNoise(pillarThickness, 0.0, 1.1);
    DensityFunctionPtr combined = DensityCommon::add(
            DensityCommon::mul(std::move(pillarNoise), DensityCommon::constant(2.0)),
            std::move(rarity)
    );
    return DensityCommon::cacheOnce(
            DensityCommon::mul(std::move(combined), DensityCommon::cube(std::move(thickness))));
}

DensityFunctionPtr OverworldCavesDensity::spaghetti2d(const DensityFunctionPtr &spaghetti2dThicknessModulator,
                                                      const NormalNoisePtr &spaghetti2dModulator,
                                                      const NormalNoisePtr &spaghetti2d,
                                                      const NormalNoisePtr &spaghetti2dElevation) {
    DensityFunctionPtr modulator = DensityCommon::noise(spaghetti2dModulator, 2.0, 1.0);
    DensityFunctionPtr sampled = DensityCommon::weirdScaledSampler(std::move(modulator), spaghetti2d,
                                                                   RarityValueMapper::TYPE2);
    DensityFunctionPtr elevation = DensityCommon::flatCache(
            DensityCommon::mul(
                    DensityCommon::constant(8.0),
                    DensityCommon::noise(spaghetti2dElevation, 1.0, 0.0)
            )
    );
    DensityFunctionPtr elevationGradient = DensityCommon::cube(
            DensityCommon::add(
                    DensityCommon::abs(
                            DensityCommon::add(std::move(elevation),
                                               DensityCommon::yClampedGradient(-64, 320, 8.0, -40.0))),
                    spaghetti2dThicknessModulator
            )
    );
    DensityFunctionPtr adjusted = DensityCommon::add(
            std::move(sampled),
            DensityCommon::mul(DensityCommon::constant(0.083), spaghetti2dThicknessModulator)
    );
    return DensityCommon::clamp(DensityCommon::max(std::move(adjusted), std::move(elevationGradient)), -1.0, 1.0);
}

DensityFunctionPtr OverworldCavesDensity::noodle(const NormalNoisePtr &noodle, const NormalNoisePtr &noodleThickness,
                                                 const NormalNoisePtr &noodleRidgeA,
                                                 const NormalNoisePtr &noodleRidgeB) {
    DensityFunctionPtr toggle = _yLimitedInterpolatable(
            DensityCommon::noise(noodle, 1.0, 1.0),
            -60,
            320,
            -1.0
    );
    DensityFunctionPtr thickness = _yLimitedInterpolatable(
            DensityCommon::mappedNoise(noodleThickness, 1.0, 1.0, -0.05, -0.1),
            -60,
            320,
            0.0
    );
    DensityFunctionPtr ridgeA = _yLimitedInterpolatable(
            DensityCommon::noise(noodleRidgeA, 2.6666666666666665, 2.6666666666666665),
            -60,
            320,
            0.0
    );
    DensityFunctionPtr ridgeB = _yLimitedInterpolatable(
            DensityCommon::noise(noodleRidgeB, 2.6666666666666665, 2.6666666666666665),
            -60,
            320,
            0.0
    );
    DensityFunctionPtr ridges = DensityCommon::mul(
            DensityCommon::constant(1.5),
            DensityCommon::max(DensityCommon::abs(std::move(ridgeA)), DensityCommon::abs(std::move(ridgeB)))
    );
    return DensityCommon::rangeChoice(
            std::move(toggle),
            -1000000.0,
            0.0,
            DensityCommon::constant(64.0),
            DensityCommon::add(std::move(thickness), std::move(ridges))
    );
}

DensityFunctionPtr OverworldCavesDensity::finalDensity(const DensityFunctionPtr &slopedCheese,
                                                       const NormalNoisePtr &spaghettiRoughness,
                                                       const NormalNoisePtr &spaghettiRoughnessModulator,
                                                       const NormalNoisePtr &spaghetti2dThickness,
                                                       const NormalNoisePtr &spaghetti2dModulator,
                                                       const NormalNoisePtr &spaghetti2d,
                                                       const NormalNoisePtr &spaghetti2dElevation,
                                                       const NormalNoisePtr &spaghetti3dRarity,
                                                       const NormalNoisePtr &spaghetti3dThickness,
                                                       const NormalNoisePtr &spaghetti3dFirst,
                                                       const NormalNoisePtr &spaghetti3dSecond,
                                                       const NormalNoisePtr &caveEntrance,
                                                       const NormalNoisePtr &caveLayer,
                                                       const NormalNoisePtr &caveCheese,
                                                       const NormalNoisePtr &pillar,
                                                       const NormalNoisePtr &pillarRareness,
                                                       const NormalNoisePtr &pillarThickness,
                                                       const NormalNoisePtr &noodle,
                                                       const NormalNoisePtr &noodleThickness,
                                                       const NormalNoisePtr &noodleRidgeA,
                                                       const NormalNoisePtr &noodleRidgeB) {
    DensityFunctionPtr spaghettiRoughnessFn = spaghettiRoughnessFunction(spaghettiRoughness,
                                                                         spaghettiRoughnessModulator);
    DensityFunctionPtr spaghetti2dThicknessModulatorFn = spaghetti2dThicknessModulator(spaghetti2dThickness);
    DensityFunctionPtr spaghetti2dFunction = OverworldCavesDensity::spaghetti2d(
            spaghetti2dThicknessModulatorFn,
            spaghetti2dModulator,
            spaghetti2d,
            spaghetti2dElevation
    );
    DensityFunctionPtr entrancesFn = entrances(
            spaghettiRoughnessFn,
            spaghetti3dRarity,
            spaghetti3dThickness,
            spaghetti3dFirst,
            spaghetti3dSecond,
            caveEntrance
    );
    DensityFunctionPtr pillarsFn = pillars(pillar, pillarRareness, pillarThickness);
    DensityFunctionPtr underground = _underground(
            slopedCheese,
            spaghetti2dFunction,
            spaghettiRoughnessFn,
            entrancesFn,
            pillarsFn,
            caveLayer,
            caveCheese
    );
    DensityFunctionPtr caves = DensityCommon::rangeChoice(
            slopedCheese,
            -1000000.0,
            1.5625,
            DensityCommon::min(
                    slopedCheese,
                    DensityCommon::mul(DensityCommon::constant(5.0), entrancesFn)
            ),
            std::move(underground)
    );
    DensityFunctionPtr postProcessed = DensityCommon::squeeze(
            DensityCommon::mul(
                    DensityCommon::constant(0.64),
                    DensityCommon::interpolated(
                            DensityCommon::blendDensity(
                                    DensityCommon::add(
                                            DensityCommon::constant(0.1171875),
                                            DensityCommon::mul(
                                                    DensityCommon::yClampedGradient(-64, -40, 0.0, 1.0),
                                                    DensityCommon::add(
                                                            DensityCommon::constant(-0.1171875),
                                                            DensityCommon::add(
                                                                    DensityCommon::constant(-0.078125),
                                                                    DensityCommon::mul(
                                                                            DensityCommon::yClampedGradient(240, 256,
                                                                                                            1.0, 0.0),
                                                                            DensityCommon::add(
                                                                                    DensityCommon::constant(0.078125),
                                                                                    std::move(caves)
                                                                            )
                                                                    )
                                                            )
                                                    )
                                            )
                                    )
                            )
                    )
            )
    );
    return DensityCommon::min(std::move(postProcessed),
                              OverworldCavesDensity::noodle(noodle, noodleThickness, noodleRidgeA, noodleRidgeB));
}

DensityFunctionPtr OverworldCavesDensity::preliminarySurfaceLevel(const DensityFunctionPtr &offset,
                                                                  const DensityFunctionPtr &factor) {
    DensityFunctionPtr base = DensityCommon::add(
            DensityCommon::yClampedGradient(-64, 320, 1.5, -1.5),
            DensityCommon::cache2d(offset)
    );
    DensityFunctionPtr scaled = DensityCommon::quarterNegative(
            DensityCommon::mul(std::move(base), DensityCommon::cache2d(factor)));
    DensityFunctionPtr clamped = DensityCommon::clamp(
            DensityCommon::add(
                    DensityCommon::constant(-0.703125),
                    DensityCommon::mul(DensityCommon::constant(4.0), std::move(scaled))
            ), -64.0, 64.0);
    return DensityCommon::add(
            DensityCommon::constant(-0.390625),
            DensityCommon::add(
                    DensityCommon::constant(0.1171875),
                    DensityCommon::mul(
                            DensityCommon::yClampedGradient(-64, -40, 0.0, 1.0),
                            DensityCommon::add(
                                    DensityCommon::constant(-0.1171875),
                                    DensityCommon::add(
                                            DensityCommon::constant(-0.078125),
                                            DensityCommon::mul(
                                                    DensityCommon::yClampedGradient(240, 256, 1.0, 0.0),
                                                    DensityCommon::add(
                                                            DensityCommon::constant(0.078125),
                                                            std::move(clamped)
                                                    )
                                            )
                                    )
                            )
                    )
            )
    );
}

DensityFunctionPtr OverworldCavesDensity::preliminarySurfaceLevelUpperBound(const DensityFunctionPtr &offset,
                                                                            const DensityFunctionPtr &factor) {
    return DensityCommon::clamp(
            DensityCommon::add(
                    DensityCommon::constant(128.0),
                    DensityCommon::mul(
                            DensityCommon::constant(-128.0),
                            DensityCommon::add(
                                    DensityCommon::mul(
                                            DensityCommon::constant(0.2734375),
                                            DensityCommon::invert(DensityCommon::cache2d(factor))
                                    ),
                                    DensityCommon::mul(
                                            DensityCommon::constant(-1.0),
                                            DensityCommon::cache2d(offset)
                                    )
                            )
                    )
            ), -40.0, 320.0);
}

DensityFunctionPtr OverworldCavesDensity::_underground(const DensityFunctionPtr &slopedCheese,
                                                       const DensityFunctionPtr &spaghetti2d,
                                                       const DensityFunctionPtr &spaghettiRoughnessFunction,
                                                       const DensityFunctionPtr &entrances,
                                                       const DensityFunctionPtr &pillars,
                                                       const NormalNoisePtr &caveLayer,
                                                       const NormalNoisePtr &caveCheese) {
    DensityFunctionPtr layerizedCaverns = DensityCommon::mul(
            DensityCommon::constant(4.0),
            DensityCommon::square(DensityCommon::noise(caveLayer, 1.0, 8.0))
    );
    DensityFunctionPtr caveCheeseFunction = DensityCommon::add(
            DensityCommon::clamp(
                    DensityCommon::add(
                            DensityCommon::constant(0.27),
                            DensityCommon::noise(caveCheese, 1.0, 0.6666666666666666)
                    ), -1.0, 1.0),
            DensityCommon::clamp(
                    DensityCommon::add(
                            DensityCommon::constant(1.5),
                            DensityCommon::mul(DensityCommon::constant(-0.64), slopedCheese)
                    ), 0.0, 0.5)
    );
    DensityFunctionPtr caveDensity = DensityCommon::add(std::move(layerizedCaverns), std::move(caveCheeseFunction));
    DensityFunctionPtr passages = DensityCommon::min(
            DensityCommon::min(std::move(caveDensity), entrances),
            DensityCommon::add(spaghetti2d, spaghettiRoughnessFunction)
    );
    DensityFunctionPtr pillarFilter = DensityCommon::rangeChoice(
            pillars,
            -1000000.0,
            0.03,
            DensityCommon::constant(-1000000.0),
            pillars
    );
    return DensityCommon::max(std::move(passages), std::move(pillarFilter));
}

DensityFunctionPtr OverworldCavesDensity::_yLimitedInterpolatable(DensityFunctionPtr density, int32_t minY,
                                                                  int32_t maxY, double whenOutOfRange) {
    return DensityCommon::interpolated(
            DensityCommon::rangeChoice(
                    DensityCommon::blockY(),
                    minY,
                    maxY + 1.0,
                    std::move(density),
                    DensityCommon::constant(whenOutOfRange)
            )
    );
}
