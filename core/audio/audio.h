#pragma once

#include <memory>
#include <string>

namespace core::audio {

// A small streaming player intended for app music, narration, and sound beds.
// All control functions are expected to run on the UI thread.
class Player {
public:
    Player();
    ~Player();

    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;
    Player(Player&&) noexcept;
    Player& operator=(Player&&) noexcept;

    // Opens an audio file without starting it. MP3, WAV, FLAC, and OGG support
    // is provided by miniaudio's built-in decoders.
    bool load(const std::string& path);
    void play();
    void pause();
    void stop();
    bool seek(double seconds);
    void unload();

    bool loaded() const;
    bool playing() const;
    bool finished() const;
    double positionSeconds() const;
    double durationSeconds() const;
    const std::string& error() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace core::audio
