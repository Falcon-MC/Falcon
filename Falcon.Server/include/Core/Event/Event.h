#pragma once

class Event {
public:
    virtual ~Event() = default;
};

class CancellableEvent : public Event {
public:
    bool isCancelled() const {
        return mCancelled;
    }

    void cancel() {
        mCancelled = true;
    }

    void setCancelled(bool cancelled) {
        mCancelled = cancelled;
    }

private:
    bool mCancelled = false;
};
