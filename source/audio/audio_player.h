#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <3ds.h>
#include <stdbool.h>

#define AUDIO_MAX_WAV_SIZE  (8 * 1024 * 1024)  // 8MB max PCM data

typedef struct {
    // WAV data (in linear memory for NDSP)
    void*       pcm_data;
    u32         data_size;
    u32         sample_rate;
    u32         total_samples;
    u16         channels;
    u16         bits_per_sample;
    u16         block_align;
    ndspWaveBuf wav_buf;

    // Playback state
    bool ndsp_ok;
    bool loaded;
    bool playing;
    bool paused;
    u32  seek_base;     // sample offset from seek operations
    bool loop;
} AudioPlayer;

void audio_player_init(AudioPlayer* player);
void audio_player_shutdown(AudioPlayer* player);

// Load a WAV from SD card. Returns false on error.
bool audio_player_load(AudioPlayer* player, const char* path);
void audio_player_unload(AudioPlayer* player);

void audio_player_play(AudioPlayer* player);
void audio_player_stop(AudioPlayer* player);
void audio_player_toggle_pause(AudioPlayer* player);

// Seek by offset_sec (positive = forward, negative = back)
void audio_player_seek(AudioPlayer* player, int offset_sec);

// Set loop mode (true = loop, false = no loop)
void audio_player_set_loop(AudioPlayer* player, bool loop);

// Current position and duration in seconds
float audio_player_position_sec(const AudioPlayer* player);
float audio_player_duration_sec(const AudioPlayer* player);

// True if finished playing (reached end)
bool audio_player_finished(const AudioPlayer* player);

// Restart playback from the beginning
void audio_player_restart(AudioPlayer* player);

#endif // AUDIO_PLAYER_H
