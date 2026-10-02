#include "Plugin/PluginRegistrationScope.h"

namespace {
    const void *gOwner = nullptr;
    bool gActive = true;
}

PluginRegistrationScope::PluginRegistrationScope(const void *owner, bool active)
        : mPreviousOwner(gOwner), mPreviousActive(gActive) {
    gOwner = owner;
    gActive = active;
}

PluginRegistrationScope::~PluginRegistrationScope() {
    gOwner = mPreviousOwner;
    gActive = mPreviousActive;
}

const void *PluginRegistrationScope::getOwner() {
    return gOwner;
}

bool PluginRegistrationScope::isActive() {
    return gOwner == nullptr || gActive;
}
