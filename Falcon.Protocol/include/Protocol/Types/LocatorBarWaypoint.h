#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Utility/UUID.h"

#include <cstdint>
#include <optional>
#include <string>

class LocatorBarWorldPosition {
public:
    Vector3f mPosition;
    int32_t mDimension = 0;
};

class LocatorBarWaypoint {
public:
    int32_t mUpdateFlag = 0;
    std::optional<bool> mVisible;
    std::optional<LocatorBarWorldPosition> mWorldPosition;
    std::optional<std::string> mTexturePath;
    std::optional<Vector2f> mIconSize;
    std::optional<int32_t> mColor;
    std::optional<bool> mClientPositionAuthority;
    std::optional<int64_t> mActorUniqueId;
};

enum class LocatorBarAction {
    None,
    Add,
    Remove,
    Update
};

class LocatorBarPayload {
public:
    LocatorBarAction mActionFlag = LocatorBarAction::None;
    Uuid mGroupHandle;
    LocatorBarWaypoint mWaypoint;
};
