#pragma once
#include "game.hpp"
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
namespace bomber {
// [01] Asynchronous generated PCM audio; no WAV files or third-party audio library.
class Audio {
public:
    bool start();
    void play(Event event);
    void mute(bool value);
    void stop();
    ~Audio() { stop(); }
private:
    std::atomic<bool> running{false},muted{false};
    std::thread worker;
    std::mutex mutex;
    std::vector<Event> pending;
    void loop();
};
} // namespace bomber
