#include "World/Village/PoiInstance.h"

PoiInstance::PoiInstance(const Vector3i &position, const PoiBlueprint &blueprint)
    : mPosition(position), mName(blueprint.mName), mInitEvent(blueprint.mInitEvent),
      mEndEvent(blueprint.mEndEvent), mType(blueprint.mType), mOwnerCount(0), mCapacity(blueprint.mCapacity),
      mWeight(blueprint.mWeight) {
}
