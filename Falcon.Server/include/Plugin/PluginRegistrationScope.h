#pragma once

class PluginRegistrationScope {
public:
    PluginRegistrationScope(const void *owner, bool active);

    ~PluginRegistrationScope();

    PluginRegistrationScope(const PluginRegistrationScope &) = delete;

    PluginRegistrationScope &operator=(const PluginRegistrationScope &) = delete;

    static const void *getOwner();

    static bool isActive();

private:
    const void *mPreviousOwner;
    bool mPreviousActive;
};
