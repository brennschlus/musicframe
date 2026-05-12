#include "moment_store.h"
#include "../image/image_buffer.h"
#include "../image/image_filters.h"
#include <3ds.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

// ---------------------------------------------------------------------------
// Binary format constants
// ---------------------------------------------------------------------------

#define MAGIC   "MFMM"
#define VERSION 1

// Packed header written/read as individual fields to avoid struct padding issues.
// Layout:
//   [4] magic
//   [1] version
//   [4] filter_id  (int32 LE)
//   [4] frame_id   (int32 LE)
//   [2] img_w      (uint16 LE)
//   [2] img_h      (uint16 LE)
//   [4] img_size   (uint32 LE) = w*h*4
//   [N] img_data   (u32 RGBA8 pixels, from scene->original)
//   [2] path_len   (uint16 LE, includes null terminator; 0 = no music)
//   [M] music_path (null-terminated full sdmc: path)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Seconds since Jan 1, 2000 that osGetTime() uses as its epoch.
#define EPOCH_2000_TO_UNIX 946684800ULL

static void build_filename(char *buf, size_t buf_size)
{
    u64 ms       = osGetTime();
    time_t unix  = (time_t)(ms / 1000) + EPOCH_2000_TO_UNIX;
    struct tm *t = gmtime(&unix);
    if (t) {
        snprintf(buf, buf_size, "%04d%02d%02d_%02d%02d%02d.moment",
                 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                 t->tm_hour, t->tm_min, t->tm_sec);
    } else {
        snprintf(buf, buf_size, "%llu.moment", (unsigned long long)(ms / 1000));
    }
}

static bool has_moment_extension(const char *name)
{
    size_t len = strlen(name);
    if (len < 8) return false; // "x.moment" = 8 chars minimum
    return strcmp(name + len - 7, ".moment") == 0;
}

// ---------------------------------------------------------------------------
// moment_store_save
// ---------------------------------------------------------------------------

int moment_store_save(const SceneModel *scene)
{
    if (!scene || !scene->photo_loaded || !scene->original) return -1;

    // Ensure the moments directory exists
    mkdir(MOMENTS_DIR, 0777); // ignore error (dir may already exist)

    char filename[64];
    build_filename(filename, sizeof(filename));

    char path[256];
    snprintf(path, sizeof(path), "%s%s", MOMENTS_DIR, filename);

    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    int32_t  filter_id = (int32_t)scene->selected_filter;
    int32_t  frame_id  = (int32_t)scene->selected_frame;
    uint16_t img_w     = scene->original->width;
    uint16_t img_h     = scene->original->height;
    uint32_t img_size  = (uint32_t)img_w * img_h * sizeof(u32);
    uint8_t  version   = VERSION;

    // Prepare music path field
    uint16_t path_len = 0;
    const char *music_path = "";
    if (scene->music_selected && scene->music_path[0] != '\0') {
        path_len   = (uint16_t)(strlen(scene->music_path) + 1);
        music_path = scene->music_path;
    }

    bool ok =
        fwrite(MAGIC,      1, 4, f) == 4 &&
        fwrite(&version,   1, 1, f) == 1 &&
        fwrite(&filter_id, 4, 1, f) == 1 &&
        fwrite(&frame_id,  4, 1, f) == 1 &&
        fwrite(&img_w,     2, 1, f) == 1 &&
        fwrite(&img_h,     2, 1, f) == 1 &&
        fwrite(&img_size,  4, 1, f) == 1 &&
        fwrite(scene->original->pixels, 1, img_size, f) == img_size &&
        fwrite(&path_len,  2, 1, f) == 1 &&
        (path_len == 0 || fwrite(music_path, 1, path_len, f) == path_len);

    fclose(f);
    if (!ok) {
        remove(path); // clean up partial file
        return -1;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// moment_store_list
// ---------------------------------------------------------------------------

// Read only the header fields up to (but not including) img_data.
// Fills info->filter_id, info->frame_id, info->music_name on success.
static bool read_header(const char *full_path, MomentInfo *info)
{
    FILE *f = fopen(full_path, "rb");
    if (!f) return false;

    char     magic[4];
    uint8_t  version;
    int32_t  filter_id, frame_id;
    uint16_t img_w, img_h;
    uint32_t img_size;

    bool ok =
        fread(magic,      1, 4, f) == 4 && memcmp(magic, MAGIC, 4) == 0 &&
        fread(&version,   1, 1, f) == 1 && version == VERSION         &&
        fread(&filter_id, 4, 1, f) == 1 &&
        fread(&frame_id,  4, 1, f) == 1 &&
        fread(&img_w,     2, 1, f) == 1 &&
        fread(&img_h,     2, 1, f) == 1 &&
        fread(&img_size,  4, 1, f) == 1;

    if (!ok) { fclose(f); return false; }

    // Skip pixel data
    if (fseek(f, img_size, SEEK_CUR) != 0) { fclose(f); return false; }

    // Read music path
    uint16_t path_len = 0;
    info->music_name[0] = '\0';
    if (fread(&path_len, 2, 1, f) == 1 && path_len > 0) {
        char music_path[256];
        size_t to_read = path_len < (uint16_t)sizeof(music_path)
                         ? path_len : sizeof(music_path) - 1;
        if (fread(music_path, 1, to_read, f) == to_read) {
            music_path[to_read] = '\0';
            const char *name = strrchr(music_path, '/');
            name = name ? name + 1 : music_path;
            strncpy(info->music_name, name, sizeof(info->music_name) - 1);
            info->music_name[sizeof(info->music_name) - 1] = '\0';
        }
    }

    fclose(f);
    info->filter_id = (int)filter_id;
    info->frame_id  = (int)frame_id;
    return true;
}

int moment_store_list(MomentInfo *out, int max_count)
{
    int count = 0;

    DIR *dir = opendir(MOMENTS_DIR);
    if (!dir) return 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && count < max_count) {
        if (entry->d_type == DT_DIR) continue;
        if (!has_moment_extension(entry->d_name)) continue;

        MomentInfo *info = &out[count];
        strncpy(info->filename, entry->d_name, sizeof(info->filename) - 1);
        info->filename[sizeof(info->filename) - 1] = '\0';

        char full_path[256];
        snprintf(full_path, sizeof(full_path), "%s%s", MOMENTS_DIR, entry->d_name);

        if (read_header(full_path, info)) {
            count++;
        }
    }

    closedir(dir);
    return count;
}

// ---------------------------------------------------------------------------
// moment_store_load
// ---------------------------------------------------------------------------

int moment_store_load(const char *filename, SceneModel *scene)
{
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s%s", MOMENTS_DIR, filename);

    FILE *f = fopen(full_path, "rb");
    if (!f) return -1;

    char     magic[4];
    uint8_t  version;
    int32_t  filter_id, frame_id;
    uint16_t img_w, img_h;
    uint32_t img_size;

    bool ok =
        fread(magic,      1, 4, f) == 4 && memcmp(magic, MAGIC, 4) == 0 &&
        fread(&version,   1, 1, f) == 1 && version == VERSION         &&
        fread(&filter_id, 4, 1, f) == 1 &&
        fread(&frame_id,  4, 1, f) == 1 &&
        fread(&img_w,     2, 1, f) == 1 &&
        fread(&img_h,     2, 1, f) == 1 &&
        fread(&img_size,  4, 1, f) == 1;

    if (!ok) { fclose(f); return -1; }

    // Sanity-check: size must match dimensions
    if (img_size != (uint32_t)img_w * img_h * sizeof(u32)) {
        fclose(f);
        return -1;
    }

    ImageBuffer *buf = image_buffer_create(img_w, img_h);
    if (!buf) { fclose(f); return -1; }

    if (fread(buf->pixels, 1, img_size, f) != img_size) {
        image_buffer_destroy(buf);
        fclose(f);
        return -1;
    }

    // Read music path
    char music_path[256];
    music_path[0] = '\0';
    uint16_t path_len = 0;
    if (fread(&path_len, 2, 1, f) == 1 && path_len > 0) {
        size_t to_read = path_len < (uint16_t)sizeof(music_path)
                         ? path_len : sizeof(music_path) - 1;
        if (fread(music_path, 1, to_read, f) != to_read) {
            music_path[0] = '\0';
        } else {
            music_path[to_read] = '\0';
        }
    }
    fclose(f);

    // Populate scene — set_photo resets filter/frame to defaults and uploads texture
    scene_model_set_photo(scene, buf);

    // Restore saved filter and frame, re-apply
    scene->selected_filter = (int)filter_id;
    scene->selected_frame  = (int)frame_id;
    scene_model_apply_filter(scene);

    // Set music
    if (music_path[0] != '\0') {
        strncpy(scene->music_path, music_path, sizeof(scene->music_path) - 1);
        scene->music_path[sizeof(scene->music_path) - 1] = '\0';
        scene->music_selected = true;

        // Verify the music file is still present
        FILE *mf = fopen(scene->music_path, "rb");
        if (!mf) {
            scene->music_selected = false;
            return -2;
        }
        fclose(mf);
    } else {
        scene->music_path[0]  = '\0';
        scene->music_selected = false;
    }

    return 0;
}
