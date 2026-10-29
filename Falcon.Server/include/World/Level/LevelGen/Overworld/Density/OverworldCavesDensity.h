#pragma once

#include "World/Level/LevelGen/Density/DensityCommon.h"

class OverworldCavesDensity {
public:
    static DensityFunctionPtr spaghettiRoughnessFunction(const NormalNoisePtr &spaghettiRoughness,
                                                         const NormalNoisePtr &spaghettiRoughnessModulator);

    static DensityFunctionPtr spaghetti2dThicknessModulator(const NormalNoisePtr &spaghetti2dThickness);

    static DensityFunctionPtr entrances(const DensityFunctionPtr &spaghettiRoughnessFunction,
                                        const NormalNoisePtr &spaghetti3dRarity,
                                        const NormalNoisePtr &spaghetti3dThickness,
                                        const NormalNoisePtr &spaghetti3dFirst,
                                        const NormalNoisePtr &spaghetti3dSecond,
                                        const NormalNoisePtr &caveEntrance);

    static DensityFunctionPtr pillars(const NormalNoisePtr &pillar, const NormalNoisePtr &pillarRareness,
                                      const NormalNoisePtr &pillarThickness);

    static DensityFunctionPtr spaghetti2d(const DensityFunctionPtr &spaghetti2dThicknessModulator,
                                          const NormalNoisePtr &spaghetti2dModulator,
                                          const NormalNoisePtr &spaghetti2d,
                                          const NormalNoisePtr &spaghetti2dElevation);

    static DensityFunctionPtr noodle(const NormalNoisePtr &noodle, const NormalNoisePtr &noodleThickness,
                                     const NormalNoisePtr &noodleRidgeA, const NormalNoisePtr &noodleRidgeB);

    static DensityFunctionPtr finalDensity(const DensityFunctionPtr &slopedCheese,
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
                                           const NormalNoisePtr &noodleRidgeB);

    static DensityFunctionPtr preliminarySurfaceLevel(const DensityFunctionPtr &offset,
                                                      const DensityFunctionPtr &factor);

    static DensityFunctionPtr preliminarySurfaceLevelUpperBound(const DensityFunctionPtr &offset,
                                                                const DensityFunctionPtr &factor);

private:
    static DensityFunctionPtr _underground(const DensityFunctionPtr &slopedCheese,
                                           const DensityFunctionPtr &spaghetti2d,
                                           const DensityFunctionPtr &spaghettiRoughnessFunction,
                                           const DensityFunctionPtr &entrances, const DensityFunctionPtr &pillars,
                                           const NormalNoisePtr &caveLayer, const NormalNoisePtr &caveCheese);

    static DensityFunctionPtr _yLimitedInterpolatable(DensityFunctionPtr density, int32_t minY, int32_t maxY,
                                                      double whenOutOfRange);
};
