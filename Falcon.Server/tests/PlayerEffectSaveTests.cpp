#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/MobEffect.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // effects above id 30 and amplifiers above 127 come back from the save as they went in
    void run() {
        ServerNetworkHandler owner("Player effect save", "", 1);
        NetworkIdentifier id;
        ServerPlayer player(id, 5001);
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        MobEffectInstance weaving;
        weaving.mId = MobEffectId::Weaving;
        weaving.mDuration = 400;
        weaving.mAmplifier = 200;
        player.addEffect(weaving);
        MobEffectInstance speed;
        speed.mId = MobEffectId::Speed;
        speed.mDuration = 400;
        speed.mAmplifier = 1;
        player.addEffect(speed);

        const Tag saved = player.saveNbt("world");
        ServerPlayer loaded(id, 5002);
        loaded.getAttributes() = ActorAttributes::createPlayerDefaults();
        loaded.loadNbt(saved, owner.getCodecContext());

        const MobEffectInstance *restored = loaded.getEffect(MobEffectId::Weaving);
        require(restored != nullptr, "weaving did not survive the save");
        require(restored->mAmplifier == 200,
                "an amplifier above 127 came back as " + std::to_string(restored->mAmplifier));
        const MobEffectInstance *fast = loaded.getEffect(MobEffectId::Speed);
        require(fast != nullptr && fast->mAmplifier == 1, "speed did not survive the save");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-player-effects-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run();
    } catch (const std::exception &error) {
        std::clog << "Player effect save regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
