#include "Network/ClientProbe.h"

#include <iostream>
#include <stdexcept>

namespace {
    using State = ClientProbeProgress::State;

    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // ticks until the probe is sent, the player staying in the state it needs
    int64_t send(ClientProbeProgress &progress, int64_t tick) {
        while (!progress.tick(true, tick))
            tick++;
        return tick;
    }

    void waitsForASteadyState() {
        ClientProbeProgress progress;
        for (int tick = 0; tick < ClientProbeProgress::READY_TICKS - 1; tick++)
            require(!progress.tick(true, tick), "sent before the state settled");

        require(!progress.tick(false, 20), "sent while the player was not ready");
        for (int tick = 21; tick < 21 + ClientProbeProgress::READY_TICKS - 1; tick++)
            require(!progress.tick(true, tick), "an interrupted wait was not started over");
        require(progress.tick(true, 40) && progress.mState == State::Sent, "never sent");
    }

    void rightAnswerPasses() {
        ClientProbeProgress progress;
        const int64_t sent = send(progress, 0);
        progress.answered(true);
        require(progress.mState == State::Passed, "a right answer did not pass");
        require(!progress.tick(true, sent + 1000), "a passed probe was sent again");
    }

    void wrongAnswerFails() {
        ClientProbeProgress progress;
        send(progress, 0);
        progress.answered(false);
        require(progress.mState == State::Failed, "a wrong answer did not fail");
    }

    void silenceFailsOnlyAfterEveryAttempt() {
        ClientProbeProgress progress;
        int64_t tick = 0;
        for (int attempt = 1; attempt <= ClientProbeProgress::MAX_ATTEMPTS; attempt++) {
            tick = send(progress, tick);
            require(progress.mAttempts == attempt, "an attempt was not counted");
            require(!progress.tick(true, tick + ClientProbeProgress::ANSWER_TICKS - 1)
                            && progress.mState == State::Sent,
                    "gave up before the deadline");

            tick += ClientProbeProgress::ANSWER_TICKS;
            progress.tick(true, tick);
            require((progress.mState == State::Failed) == (attempt == ClientProbeProgress::MAX_ATTEMPTS),
                    "silence was judged at the wrong attempt");
        }
    }

    void closingTheScreenVoidsTheAttempt() {
        ClientProbeProgress progress;
        int64_t tick = 0;
        for (int round = 0; round < ClientProbeProgress::MAX_ATTEMPTS * 3; round++) {
            tick = send(progress, tick) + 1;
            progress.tick(false, tick);
            require(progress.mState == State::Waiting && progress.mAttempts == 0,
                    "a screen closed mid-question was held against the player");
        }
    }

    void lateAnswerIsIgnored() {
        ClientProbeProgress progress;
        progress.answered(true);
        require(progress.mState == State::Waiting, "an answer to nothing was counted");
    }
}

int main() {
    try {
        waitsForASteadyState();
        rightAnswerPasses();
        wrongAnswerFails();
        silenceFailsOnlyAfterEveryAttempt();
        closingTheScreenVoidsTheAttempt();
        lateAnswerIsIgnored();
    } catch (const std::exception &error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
