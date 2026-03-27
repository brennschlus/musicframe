#include "music_library.h"
#include <dirent.h>
#include <string.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
static bool has_wav_extension(const char* name)
{
    size_t len = strlen(name);
    if (len < 5) return false;  // minimum: "a.wav"
    const char* ext = name + len - 4;
    return (ext[0] == '.' &&
           (ext[1] == 'w' || ext[1] == 'W') &&
           (ext[2] == 'a' || ext[2] == 'A') &&
           (ext[3] == 'v' || ext[3] == 'V'));
}

// ---------------------------------------------------------------------------
int music_library_scan(MusicLibrary* lib)
{
    lib->count = 0;

    DIR* dir = opendir(MUSIC_DIR);
    if (!dir) return 0;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && lib->count < MAX_MUSIC_FILES) {
        // Skip directories (d_type == DT_DIR) and non-wav files
        if (entry->d_type == DT_DIR) continue;
        if (!has_wav_extension(entry->d_name)) continue;

        strncpy(lib->filenames[lib->count], entry->d_name, MAX_FILENAME_LEN - 1);
        lib->filenames[lib->count][MAX_FILENAME_LEN - 1] = '\0';
        lib->count++;
    }

    closedir(dir);
    return lib->count;
}

// ---------------------------------------------------------------------------
const char* music_library_get_path(const MusicLibrary* lib, int index,
                                   char* buf, int buf_size)
{
    if (index < 0 || index >= lib->count) {
        buf[0] = '\0';
        return buf;
    }
    snprintf(buf, buf_size, "%s%s", MUSIC_DIR, lib->filenames[index]);
    return buf;
}
