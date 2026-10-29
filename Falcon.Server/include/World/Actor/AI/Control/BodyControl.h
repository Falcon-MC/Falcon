#pragma once

class LookControl;
class MobActor;
class MoveControl;

class BodyControl {
public:
    void tick(MobActor &mob, const MoveControl &moveControl, const LookControl &lookControl);
};
