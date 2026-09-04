#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "core/audio/audio.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace core::audio {

struct Player::Impl {
    ma_engine engine{};
    ma_sound sound{};
    bool engineReady = false;
    bool soundReady = false;
    std::string error;

    ~Impl() {
        unload();
        if (engineReady) ma_engine_uninit(&engine);
    }

    void unload() {
        if (soundReady) {
            ma_sound_uninit(&sound);
            soundReady = false;
        }
    }

    bool ensureEngine() {
        if (engineReady) return true;
        const ma_result result = ma_engine_init(nullptr, &engine);
        if (result == MA_SUCCESS) {
            engineReady = true;
            return true;
        }
        error = std::string("Unable to initialize audio output: ") +
                ma_result_description(result);
        return false;
    }
};

Player::Player() : impl_(std::make_unique<Impl>()) {}
Player::~Player() = default;
Player::Player(Player&&) noexcept = default;
Player& Player::operator=(Player&&) noexcept = default;

bool Player::load(const std::string& path) {
    impl_->unload();
    impl_->error.clear();
    if (path.empty()) {
        impl_->error = "Audio file path is empty.";
        return false;
    }
    if (!impl_->ensureEngine()) return false;

    const ma_result result = ma_sound_init_from_file(
        &impl_->engine, path.c_str(), MA_SOUND_FLAG_STREAM, nullptr, nullptr,
        &impl_->sound);
    if (result != MA_SUCCESS) {
        impl_->error = std::string("Unable to load audio file: ") +
                       ma_result_description(result);
        return false;
    }
    impl_->soundReady = true;
    return true;
}

bool Player::play() {
    if (!impl_->soundReady) {
        impl_->error = "No audio file is loaded.";
        return false;
    }
    const ma_result result = ma_sound_start(&impl_->sound);
    if (result != MA_SUCCESS) {
        impl_->error = std::string("Unable to start audio playback: ") +
                       ma_result_description(result);
        return false;
    }
    impl_->error.clear();
    return true;
}

bool Player::pause() {
    if (!impl_->soundReady) {
        impl_->error = "No audio file is loaded.";
        return false;
    }
    const ma_result result = ma_sound_stop(&impl_->sound);
    if (result != MA_SUCCESS) {
        impl_->error = std::string("Unable to pause audio playback: ") +
                       ma_result_description(result);
        return false;
    }
    impl_->error.clear();
    return true;
}

bool Player::stop() {
    if (!impl_->soundReady) {
        impl_->error = "No audio file is loaded.";
        return false;
    }
    const ma_result stopResult = ma_sound_stop(&impl_->sound);
    if (stopResult != MA_SUCCESS) {
        impl_->error = std::string("Unable to stop audio playback: ") +
                       ma_result_description(stopResult);
        return false;
    }
    const ma_result seekResult = ma_sound_seek_to_pcm_frame(&impl_->sound, 0);
    if (seekResult != MA_SUCCESS) {
        impl_->error = std::string("Unable to rewind audio playback: ") +
                       ma_result_description(seekResult);
        return false;
    }
    impl_->error.clear();
    return true;
}

bool Player::seek(double seconds) {
    if (!impl_->soundReady) {
        impl_->error = "No audio file is loaded.";
        return false;
    }
    const double clamped = std::max(0.0, std::min(seconds, durationSeconds()));
    const ma_uint32 sampleRate = ma_engine_get_sample_rate(&impl_->engine);
    const ma_uint64 frame = static_cast<ma_uint64>(clamped * sampleRate);
    const ma_result result = ma_sound_seek_to_pcm_frame(&impl_->sound, frame);
    if (result == MA_SUCCESS) {
        impl_->error.clear();
        return true;
    }
    impl_->error = std::string("Unable to seek audio playback: ") +
                   ma_result_description(result);
    return false;
}

void Player::unload() {
    impl_->unload();
    impl_->error.clear();
}

bool Player::loaded() const { return impl_->soundReady; }

bool Player::playing() const {
    return impl_->soundReady && ma_sound_is_playing(&impl_->sound) == MA_TRUE;
}

bool Player::finished() const {
    return impl_->soundReady && ma_sound_at_end(&impl_->sound) == MA_TRUE;
}

double Player::positionSeconds() const {
    if (!impl_->soundReady) return 0.0;
    ma_uint64 frame = 0;
    if (ma_sound_get_cursor_in_pcm_frames(&impl_->sound, &frame) != MA_SUCCESS) {
        return 0.0;
    }
    const ma_uint32 sampleRate = ma_engine_get_sample_rate(&impl_->engine);
    return sampleRate == 0 ? 0.0 : static_cast<double>(frame) / sampleRate;
}

double Player::durationSeconds() const {
    if (!impl_->soundReady) return 0.0;
    ma_uint64 frame = 0;
    if (ma_sound_get_length_in_pcm_frames(&impl_->sound, &frame) != MA_SUCCESS) {
        return 0.0;
    }
    const ma_uint32 sampleRate = ma_engine_get_sample_rate(&impl_->engine);
    return sampleRate == 0 ? 0.0 : static_cast<double>(frame) / sampleRate;
}

const std::string& Player::error() const { return impl_->error; }

} // namespace core::audio
