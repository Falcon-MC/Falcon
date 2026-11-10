#pragma once

#include "Core/Math/Vector3f.h"

#include <quickjs.h>

class Actor;
class BlockState;
class Level;

// the script side of ScriptRaycast: entity view rays and Dimension rays share the options and the results
namespace ScriptRayQueries {
    // a BlockFilter: include and exclude types, tags and permutations
    bool blockPassesFilter(JSContext *ctx, const BlockState &state, JSValueConst filter);

    // a BlockRaycastHit, undefined without a hit
    JSValue blockFromRay(JSContext *ctx, Level &level, const Vector3f &origin, const Vector3f &direction,
                         JSValueConst options);

    // EntityRaycastHit[] nearest first; except: the entity casting it
    JSValue entitiesFromRay(JSContext *ctx, Level &level, const Vector3f &origin, const Vector3f &direction,
                            JSValueConst options, const Actor *except);
}
