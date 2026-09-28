#include "music_player_support.h"

#include <stdio.h>
#include <string.h>

void music_playback_watch_start(music_playback_watch_t *watch, uint32_t now)
{
    watch->request_tick = now;
    watch->waiting_for_start = true;
}

bool music_playback_watch_finished(music_playback_watch_t *watch, uint32_t now,
                                   bool active, bool shutdown)
{
    if(shutdown) return true;
    if(active) {
        watch->waiting_for_start = false;
        return false;
    }
    return !watch->waiting_for_start || (uint32_t)(now - watch->request_tick) >= 1000;
}

static uint32_t read_le32(const unsigned char *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

uint32_t music_wav_duration_seconds(const char *path)
{
    FILE *file = path ? fopen(path, "rb") : NULL;
    if(!file) return 0;
    uint32_t duration = 0;
    unsigned char header[16];
    if(fseek(file, 0, SEEK_END) != 0) goto done;
    long file_size = ftell(file);
    if(file_size < 12 || fseek(file, 0, SEEK_SET) != 0 ||
       fread(header, 1, 12, file) != 12 ||
       memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) goto done;

    uint64_t end = (uint64_t)read_le32(header + 4) + 8;
    if(end > (uint64_t)file_size || end < 12) goto done;
    uint32_t byte_rate = 0;
    uint32_t block_align = 0;
    uint64_t offset = 12;
    /* Bound UI-thread work even for a file containing many metadata chunks. */
    for(unsigned chunks = 0; chunks < 64 && offset + 8 <= end; chunks++) {
        if(fread(header, 1, 8, file) != 8) break;
        uint32_t size = read_le32(header + 4);
        offset += 8;
        if(size > end - offset) break;
        if(memcmp(header, "fmt ", 4) == 0) {
            if(size < 16 || fread(header, 1, 16, file) != 16) break;
            uint32_t channels = header[2] | ((uint32_t)header[3] << 8);
            uint32_t sample_rate = read_le32(header + 4);
            uint32_t bits = header[14] | ((uint32_t)header[15] << 8);
            block_align = header[12] | ((uint32_t)header[13] << 8);
            byte_rate = read_le32(header + 8);
            if(header[0] != 1 || header[1] != 0 || channels == 0 || channels > 2 ||
               bits == 0 || bits > 32 || bits % 8 != 0 || sample_rate == 0 ||
               block_align != channels * (bits / 8) ||
               (uint64_t)sample_rate * block_align != byte_rate) break;
        } else if(memcmp(header, "data", 4) == 0) {
            if(byte_rate != 0 && size % block_align == 0) {
                duration = (uint32_t)(((uint64_t)size + byte_rate - 1) / byte_rate);
            }
            break;
        }
        offset += (uint64_t)size + (size & 1U);
        if(offset > end || fseek(file, (long)offset, SEEK_SET) != 0) break;
    }
done:
    fclose(file);
    return duration;
}
