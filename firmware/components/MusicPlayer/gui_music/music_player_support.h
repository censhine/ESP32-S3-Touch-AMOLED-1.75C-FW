#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t request_tick;
    bool waiting_for_start;
} music_playback_watch_t;

void music_playback_watch_start(music_playback_watch_t *watch, uint32_t now);
/* active includes PLAYING and PAUSE. A short/invalid file can finish between
 * polls; allow a queued request one second to start before accepting IDLE. */
bool music_playback_watch_finished(music_playback_watch_t *watch, uint32_t now,
                                   bool active, bool shutdown);

/* Returns rounded-up PCM WAV duration, or zero when unknown/unsupported. */
uint32_t music_wav_duration_seconds(const char *path);
