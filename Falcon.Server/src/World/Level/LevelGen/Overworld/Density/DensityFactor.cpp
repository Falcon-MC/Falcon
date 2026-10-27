#include "World/Level/LevelGen/Overworld/Density/DensityFactor.h"

DensityFunctionPtr DensityFactor::overworldFactor(const DensityFunctionPtr &continents,
                                                  const DensityFunctionPtr &erosion,
                                                  const DensityFunctionPtr &ridges,
                                                  const DensityFunctionPtr &ridgesFolded) {
    return DensityCommon::flatCache(
            DensityCommon::cache2d(
                    DensityCommon::add(
                            DensityCommon::constant(10.0),
                            DensityCommon::mul(
                                    DensityCommon::blendAlpha(),
                                    DensityCommon::add(
                                            DensityCommon::constant(-10.0),
                                            _factorSpline(continents, erosion, ridges, ridgesFolded)
                                    )
                            )
                    )
            )
    );
}

DensityFunctionPtr DensityFactor::_factorSpline(const DensityFunctionPtr &continents,
                                                const DensityFunctionPtr &erosion,
                                                const DensityFunctionPtr &ridges,
                                                const DensityFunctionPtr &ridgesFolded) {
    DensityFunctionPtr ridgesA = DensityCommon::spline(ridges, {
            DensityCommon::p(-0.2, 6.3, 0.0),
            DensityCommon::p(0.2, 6.25, 0.0)
    });
    DensityFunctionPtr ridgesB = DensityCommon::spline(ridges, {
            DensityCommon::p(-0.05, 6.3, 0.0),
            DensityCommon::p(0.05, 2.67, 0.0)
    });
    DensityFunctionPtr ridgesC = DensityCommon::spline(ridges, {
            DensityCommon::p(-0.05, 2.67, 0.0),
            DensityCommon::p(0.05, 6.3, 0.0)
    });
    DensityFunctionPtr ridgesD = DensityCommon::spline(ridges, {
            DensityCommon::p(-0.2, 6.3, 0.0),
            DensityCommon::p(0.2, 5.47, 0.0)
    });
    DensityFunctionPtr ridgesE = DensityCommon::spline(ridges, {
            DensityCommon::p(-0.2, 6.3, 0.0),
            DensityCommon::p(0.2, 5.08, 0.0)
    });
    DensityFunctionPtr ridgesF = DensityCommon::spline(ridges, {
            DensityCommon::p(-0.2, 6.3, 0.0),
            DensityCommon::p(0.2, 4.69, 0.0)
    });
    DensityFunctionPtr ridgesG = DensityCommon::spline(ridges, {
            DensityCommon::p(0.0, 6.25, 0.0),
            DensityCommon::p(0.1, 0.625, 0.0)
    });
    DensityFunctionPtr ridgesH = DensityCommon::spline(ridges, {
            DensityCommon::p(0.0, 5.47, 0.0),
            DensityCommon::p(0.1, 0.625, 0.0)
    });
    DensityFunctionPtr ridgesI = DensityCommon::spline(ridges, {
            DensityCommon::p(0.0, 5.08, 0.0),
            DensityCommon::p(0.1, 0.625, 0.0)
    });

    DensityFunctionPtr ridgesFoldedA = DensityCommon::spline(ridgesFolded, {
            DensityCommon::p(-0.9, 6.25, 0.0),
            DensityCommon::p(-0.69, ridgesG, 0.0)
    });
    DensityFunctionPtr ridgesFoldedB = DensityCommon::spline(ridgesFolded, {
            DensityCommon::p(-0.9, 5.47, 0.0),
            DensityCommon::p(-0.69, ridgesH, 0.0)
    });
    DensityFunctionPtr ridgesFoldedC = DensityCommon::spline(ridgesFolded, {
            DensityCommon::p(-0.9, 5.08, 0.0),
            DensityCommon::p(-0.69, ridgesI, 0.0)
    });
    DensityFunctionPtr ridgesFoldedD = DensityCommon::spline(ridgesFolded, {
            DensityCommon::p(0.45, ridgesF, 0.0),
            DensityCommon::p(0.7, 1.56, 0.0)
    });
    DensityFunctionPtr ridgesFoldedE = DensityCommon::spline(ridgesFolded, {
            DensityCommon::p(-0.7, ridgesF, 0.0),
            DensityCommon::p(-0.15, 1.37, 0.0)
    });

    DensityFunctionPtr erosion1 = DensityCommon::spline(erosion, {
            DensityCommon::p(-0.6, ridgesA, 0.0),
            DensityCommon::p(-0.5, ridgesB, 0.0),
            DensityCommon::p(-0.35, ridgesA, 0.0),
            DensityCommon::p(-0.25, ridgesA, 0.0),
            DensityCommon::p(-0.1, ridgesC, 0.0),
            DensityCommon::p(0.03, ridgesA, 0.0),
            DensityCommon::p(0.35, 6.25, 0.0),
            DensityCommon::p(0.45, ridgesFoldedA, 0.0),
            DensityCommon::p(0.55, ridgesFoldedA, 0.0),
            DensityCommon::p(0.62, 6.25, 0.0)
    });
    DensityFunctionPtr erosion2 = DensityCommon::spline(erosion, {
            DensityCommon::p(-0.6, ridgesD, 0.0),
            DensityCommon::p(-0.5, ridgesB, 0.0),
            DensityCommon::p(-0.35, ridgesD, 0.0),
            DensityCommon::p(-0.25, ridgesD, 0.0),
            DensityCommon::p(-0.1, ridgesC, 0.0),
            DensityCommon::p(0.03, ridgesD, 0.0),
            DensityCommon::p(0.35, 5.47, 0.0),
            DensityCommon::p(0.45, ridgesFoldedB, 0.0),
            DensityCommon::p(0.55, ridgesFoldedB, 0.0),
            DensityCommon::p(0.62, 5.47, 0.0)
    });
    DensityFunctionPtr erosion3 = DensityCommon::spline(erosion, {
            DensityCommon::p(-0.6, ridgesE, 0.0),
            DensityCommon::p(-0.5, ridgesB, 0.0),
            DensityCommon::p(-0.35, ridgesE, 0.0),
            DensityCommon::p(-0.25, ridgesE, 0.0),
            DensityCommon::p(-0.1, ridgesC, 0.0),
            DensityCommon::p(0.03, ridgesE, 0.0),
            DensityCommon::p(0.35, 5.08, 0.0),
            DensityCommon::p(0.45, ridgesFoldedC, 0.0),
            DensityCommon::p(0.55, ridgesFoldedC, 0.0),
            DensityCommon::p(0.62, 5.08, 0.0)
    });
    DensityFunctionPtr erosion4 = DensityCommon::spline(erosion, {
            DensityCommon::p(-0.6, ridgesF, 0.0),
            DensityCommon::p(-0.5, ridgesB, 0.0),
            DensityCommon::p(-0.35, ridgesF, 0.0),
            DensityCommon::p(-0.25, ridgesF, 0.0),
            DensityCommon::p(-0.1, ridgesC, 0.0),
            DensityCommon::p(0.03, ridgesF, 0.0),
            DensityCommon::p(0.05, ridgesFoldedD, 0.0),
            DensityCommon::p(0.4, ridgesFoldedD, 0.0),
            DensityCommon::p(0.45, ridgesFoldedE, 0.0),
            DensityCommon::p(0.55, ridgesFoldedE, 0.0),
            DensityCommon::p(0.58, 4.69, 0.0)
    });

    return DensityCommon::spline(continents, {
            DensityCommon::p(-0.19, 3.95, 0.0),
            DensityCommon::p(-0.15, erosion1, 0.0),
            DensityCommon::p(-0.1, erosion2, 0.0),
            DensityCommon::p(0.03, erosion3, 0.0),
            DensityCommon::p(0.06, erosion4, 0.0)
    });
}
