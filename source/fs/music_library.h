#ifndef MUSIC_LIBRARY_H
#define MUSIC_LIBRARY_H

#include <stdbool.h>

#define MUSIC_DIR         "sdmc:/3ds/musicframe/music/"
#define MAX_MUSIC_FILES   32
#define MAX_FILENAME_LEN  128

typedef struct {
    char filenames[MAX_MUSIC_FILES][MAX_FILENAME_LEN];
    int  count;
} MusicLibrary;

// Scan MUSIC_DIR for .wav files (flat, no recursion).
// Returns number of files found.
int music_library_scan(MusicLibrary* lib);

// Build full path for file at index. Returns buf.
const char* music_library_get_path(const MusicLibrary* lib, int index,
                                   char* buf, int buf_size);

#endif // MUSIC_LIBRARY_H
