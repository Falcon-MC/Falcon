#include "World/Level/LevelGen/Overworld/Density/DensityJaggedness.h"

DensityFunctionPtr DensityJaggedness::overworldJaggedness(const DensityFunctionPtr &continents,
                                                          const DensityFunctionPtr &erosion,
                                                          const DensityFunctionPtr &ridges,
                                                          const DensityFunctionPtr &ridgesFolded) {
    return DensityCommon::flatCache(
            DensityCommon::cache2d(
                    DensityCommon::add(
                            DensityCommon::constant(0.0),
                            DensityCommon::mul(
                                    DensityCommon::blendAlpha(),
                                    DensityCommon::add(
                                            DensityCommon::constant(-0.0),
                                            _jaggednessSpline(continents, erosion, ridges, ridgesFolded)
                                    )
                            )
                    )
            )
    );
}

DensityFunctionPtr DensityJaggedness::_jaggednessSpline(const DensityFunctionPtr &continents,
                                                        const DensityFunctionPtr &erosion,
                                                        const DensityFunctionPtr &ridges,
                                                        const DensityFunctionPtr &ridgesFolded) {
    DensityFunctionPtr ridges1 = DensityCommon::spline(ridges, {
            DensityCommon::p(-0.01, 0.63, 0.0),
            DensityCommon::p(0.01, 0.3, 0.0)
    });
    DensityFunctionPtr ridges2 = DensityCommon::spline(ridges, {
            DensityCommon::p(-0.01, 0.315, 0.0),
            DensityCommon::p(0.01, 0.15, 0.0)
    });
    DensityFunctionPtr ridges3 = DensityCommon::spline(ridges, {
            DensityCommon::p(-0.01, 0.315, 0.0),
            DensityCommon::p(0.01, 0.15, 0.0)
    });
    DensityFunctionPtr ridges4 = DensityCommon::spline(ridges, {
            DensityCommon::p(-0.01, 0.63, 0.0),
            DensityCommon::p(0.01, 0.3, 0.0)
    });
    DensityFunctionPtr ridges5 = DensityCommon::spline(ridges, {
            DensityCommon::p(-0.01, 0.63, 0.0),
            DensityCommon::p(0.01, 0.3, 0.0)
    });
    DensityFunctionPtr ridges6 = DensityCommon::spline(ridges, {
            DensityCommon::p(-0.01, 0.63, 0.0),
            DensityCommon::p(0.01, 0.3, 0.0)
    });

    DensityFunctionPtr ridgesFolded1 = DensityCommon::spline(ridgesFolded, {
            DensityCommon::p(0.19999999, 0.0, 0.0),
            DensityCommon::p(0.44999996, 0.0, 0.0),
            DensityCommon::p(1.0, ridges1, 0.0)
    });
    DensityFunctionPtr ridgesFolded2 = DensityCommon::spline(ridgesFolded, {
            DensityCommon::p(0.19999999, 0.0, 0.0),
            DensityCommon::p(0.44999996, 0.0, 0.0),
            DensityCommon::p(1.0, ridges2, 0.0)
    });
    DensityFunctionPtr ridgesFolded3 = DensityCommon::spline(ridgesFolded, {
            DensityCommon::p(0.19999999, 0.0, 0.0),
            DensityCommon::p(0.44999996, 0.0, 0.0),
            DensityCommon::p(1.0, ridges3, 0.0)
    });
    DensityFunctionPtr ridgesFolded4 = DensityCommon::spline(ridgesFolded, {
            DensityCommon::p(0.19999999, 0.0, 0.0),
            DensityCommon::p(0.44999996, ridges4, 0.0),
            DensityCommon::p(1.0, ridges5, 0.0)
    });
    DensityFunctionPtr ridgesFolded5 = DensityCommon::spline(ridgesFolded, {
            DensityCommon::p(0.19999999, 0.0, 0.0),
            DensityCommon::p(0.44999996, 0.0, 0.0),
            DensityCommon::p(1.0, ridges6, 0.0)
    });

    DensityFunctionPtr erosion1 = DensityCommon::spline(erosion, {
            DensityCommon::p(-1.0, ridgesFolded1, 0.0),
            DensityCommon::p(-0.78, ridgesFolded2, 0.0),
            DensityCommon::p(-0.5775, ridgesFolded3, 0.0),
            DensityCommon::p(-0.375, 0.0, 0.0)
    });
    DensityFunctionPtr erosion2 = DensityCommon::spline(erosion, {
            DensityCommon::p(-1.0, ridgesFolded4, 0.0),
            DensityCommon::p(-0.78, ridgesFolded5, 0.0),
            DensityCommon::p(-0.5775, ridgesFolded5, 0.0),
            DensityCommon::p(-0.375, 0.0, 0.0)
    });

    return DensityCommon::spline(continents, {
            DensityCommon::p(-0.11, 0.0, 0.0),
            DensityCommon::p(0.03, erosion1, 0.0),
            DensityCommon::p(0.65, erosion2, 0.0)
    });
}
