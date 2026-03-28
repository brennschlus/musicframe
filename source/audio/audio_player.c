#include "audio_player.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// WAV header parsing
// ---------------------------------------------------------------------------
typedef struct {
    u16 audio_format;
    u16 channels;
    u32 sample_rate;
    u16 block_align;
    u16 bits_per_sample;
    u32 data_size;
    long data_offset;
} WavInfo;

static bool wav_parse(FILE* f, WavInfo* info)
{
    char id[4];
    u32 size;

    // RIFF header
    if (fread(id, 1, 4, f) != 4 || memcmp(id, "RIFF", 4) != 0) return false;
    fread(&size, 4, 1, f);
    if (fread(id, 1, 4, f) != 4 || memcmp(id, "WAVE", 4) != 0) return false;

    bool got_fmt = false, got_data = false;

    // Scan chunks
    while (!got_fmt || !got_data) {
        if (fread(id, 1, 4, f) != 4) break;
        if (fread(&size, 4, 1, f) != 1) break;

        if (memcmp(id, "fmt ", 4) == 0) {
            if (size < 16) break;
            fread(&info->audio_format, 2, 1, f);
            fread(&info->channels, 2, 1, f);
            fread(&info->sample_rate, 4, 1, f);
            u32 byte_rate;
            fread(&byte_rate, 4, 1, f);
            fread(&info->block_align, 2, 1, f);
            fread(&info->bits_per_sample, 2, 1, f);
            if (size > 16) fseek(f, size - 16, SEEK_CUR);
            got_fmt = true;
        } else if (memcmp(id, "data", 4) == 0) {
            info->data_size = size;
            info->data_offset = ftell(f);
            got_data = true;
        } else {
            fseek(f, size, SEEK_CUR);
        }
    }

    if (!got_fmt || !got_data) return false;
    if (info->audio_format != 1) return false;       // PCM only
    if (info->bits_per_sample != 16) return false;    // 16-bit only
    if (info->channels < 1 || info->channels > 2) return false;
    if (info->data_size > AUDIO_MAX_WAV_SIZE) return false;

    return true;
}

// ---------------------------------------------------------------------------
// Setup NDSP channel for current WAV format
// ---------------------------------------------------------------------------
static void setup_channel(AudioPlayer* player)
{
    ndspChnWaveBufClear(0);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, (float)player->sample_rate);

    u32 fmt = (player->channels == 2)
        ? NDSP_FORMAT_STEREO_PCM16
        : NDSP_FORMAT_MONO_PCM16;
    ndspChnSetFormat(0, fmt);

    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = 1.0f;  // left
    mix[1] = 1.0f;  // right
    ndspChnSetMix(0, mix);
}

// ---------------------------------------------------------------------------
void audio_player_init(AudioPlayer* player)
{
    memset(player, 0, sizeof(AudioPlayer));
    player->loop = true;
}

// ---------------------------------------------------------------------------
void audio_player_shutdown(AudioPlayer* player)
{
    audio_player_unload(player);
}

// ---------------------------------------------------------------------------
bool audio_player_load(AudioPlayer* player, const char* path)
{
    // Unload previous
    audio_player_unload(player);

    FILE* f = fopen(path, "rb");
    if (!f) return false;

    WavInfo info;
    if (!wav_parse(f, &info)) {
        fclose(f);
        return false;
    }

    // Allocate in linear memory (required by NDSP)
    player->pcm_data = linearAlloc(info.data_size);
    if (!player->pcm_data) {
        fclose(f);
        return false;
    }

    // Read PCM data
    fseek(f, info.data_offset, SEEK_SET);
    size_t read = fread(player->pcm_data, 1, info.data_size, f);
    fclose(f);

    if (read != info.data_size) {
        linearFree(player->pcm_data);
        player->pcm_data = NULL;
        return false;
    }

    DSP_FlushDataCache(player->pcm_data, info.data_size);

    player->data_size       = info.data_size;
    player->sample_rate     = info.sample_rate;
    player->channels        = info.channels;
    player->bits_per_sample = info.bits_per_sample;
    player->block_align     = info.block_align;
    player->total_samples   = info.data_size / info.block_align;
    player->loaded          = true;
    player->playing         = false;
    player->paused          = false;
    player->seek_base       = 0;

    return true;
}

// ---------------------------------------------------------------------------
void audio_player_unload(AudioPlayer* player)
{
    if (player->playing) {
        audio_player_stop(player);
    }
    if (player->pcm_data) {
        linearFree(player->pcm_data);
        player->pcm_data = NULL;
    }
    player->loaded  = false;
    player->playing = false;
    player->paused  = false;
}

// ---------------------------------------------------------------------------
void audio_player_play(AudioPlayer* player)
{
    if (!player->loaded || player->playing) return;

    setup_channel(player);

    memset(&player->wav_buf, 0, sizeof(ndspWaveBuf));

    u32 offset_bytes = player->seek_base * player->block_align;
    player->wav_buf.data_vaddr = (u8*)player->pcm_data + offset_bytes;
    player->wav_buf.nsamples   = player->total_samples - player->seek_base;
    player->wav_buf.looping    = false;
    player->wav_buf.status     = NDSP_WBUF_FREE;

    ndspChnWaveBufAdd(0, &player->wav_buf);

    // Explicitly unpause, just in case
    ndspChnSetPaused(0, false);

    player->playing = true;
    player->paused  = false;
}

// ---------------------------------------------------------------------------
void audio_player_stop(AudioPlayer* player)
{
    if (!player->loaded) return;
    ndspChnWaveBufClear(0);
    player->playing   = false;
    player->paused    = false;
    player->seek_base = 0;
}

// ---------------------------------------------------------------------------
void audio_player_toggle_pause(AudioPlayer* player)
{
    if (!player->loaded || !player->playing) {
        // If not playing, start from beginning or current seek position
        if (player->loaded && !player->playing) {
            audio_player_play(player);
        }
        return;
    }

    player->paused = !player->paused;
    ndspChnSetPaused(0, player->paused);
}

// ---------------------------------------------------------------------------
void audio_player_seek(AudioPlayer* player, int offset_sec)
{
    if (!player->loaded) return;

    // Get current absolute sample position
    u32 cur_pos = player->seek_base;
    if (player->playing) {
        cur_pos += ndspChnGetSamplePos(0);
    }

    int new_pos = (int)cur_pos + offset_sec * (int)player->sample_rate;
    if (new_pos < 0) new_pos = 0;
    if ((u32)new_pos >= player->total_samples) new_pos = player->total_samples - 1;

    bool was_playing = player->playing && !player->paused;

    if (player->playing) {
        ndspChnWaveBufClear(0);
        player->playing = false;
    }

    player->seek_base = (u32)new_pos;

    if (was_playing) {
        audio_player_play(player);
    }
}

// ---------------------------------------------------------------------------
float audio_player_position_sec(const AudioPlayer* player)
{
    if (!player->loaded || player->sample_rate == 0) return 0.0f;

    u32 pos = player->seek_base;
    if (player->playing) {
        pos += ndspChnGetSamplePos(0);
    }
    if (pos > player->total_samples) pos = player->total_samples;

    return (float)pos / player->sample_rate;
}

// ---------------------------------------------------------------------------
float audio_player_duration_sec(const AudioPlayer* player)
{
    if (!player->loaded || player->sample_rate == 0) return 0.0f;
    return (float)player->total_samples / player->sample_rate;
}

// ---------------------------------------------------------------------------
bool audio_player_finished(const AudioPlayer* player)
{
    if (!player->loaded || !player->playing) return false;
    return player->wav_buf.status == NDSP_WBUF_DONE;
}

void audio_player_set_loop(AudioPlayer* player, bool loop){
    player->loop = loop;
}

void audio_player_restart(AudioPlayer* player){
    player->seek_base = 0;

    if (player->playing) {
        ndspChnWaveBufClear(0);
        player->playing = false;
    }
        audio_player_play(player);
}
