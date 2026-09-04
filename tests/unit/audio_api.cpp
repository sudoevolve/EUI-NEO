#include "eui/audio.h"

#include <cassert>
#include <utility>

int main() {
#if defined(EUI_ENABLE_AUDIO)
    eui::audio::Player player;
    assert(!player.loaded());
    assert(!player.playing());
    assert(!player.finished());
    assert(player.positionSeconds() == 0.0);
    assert(player.durationSeconds() == 0.0);

    eui::audio::Player moved = std::move(player);
    assert(!moved.loaded());
#endif
    return 0;
}
