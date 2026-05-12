#include "photo_library.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>

static bool has_jpg_extension(const char *name)
{
    size_t len = strlen(name);
    if (len < 5) return false; // "a.jpg" minimum
    const char *ext = name + len - 4;
    return (ext[0] == '.' &&
           (ext[1] == 'j' || ext[1] == 'J') &&
           (ext[2] == 'p' || ext[2] == 'P') &&
           (ext[3] == 'g' || ext[3] == 'G'));
}

int photo_library_scan(PhotoLibrary *lib)
{
    lib->count = 0;

    DIR *root = opendir(PHOTO_DIR);
    if (!root) return 0;

    struct dirent *subdir_entry;
    while ((subdir_entry = readdir(root)) != NULL && lib->count < MAX_PHOTOS) {
        // Accept DT_DIR and DT_UNKNOWN (FAT filesystems often return DT_UNKNOWN).
        // opendir() is the authoritative check: if it fails the entry is not a dir.
        if (subdir_entry->d_type != DT_DIR && subdir_entry->d_type != DT_UNKNOWN) continue;
        if (subdir_entry->d_name[0] == '.') continue; // skip . and ..

        // Use a larger intermediate buffer to satisfy the compiler
        char subdir_path[384];
        snprintf(subdir_path, sizeof(subdir_path), "%s%s/", PHOTO_DIR, subdir_entry->d_name);

        DIR *sub = opendir(subdir_path);
        if (!sub) continue; // entry is not a directory — skip

        struct dirent *file_entry;
        while ((file_entry = readdir(sub)) != NULL && lib->count < MAX_PHOTOS) {
            // Skip confirmed directories; for DT_UNKNOWN let the extension check decide.
            if (file_entry->d_type == DT_DIR) continue;
            if (!has_jpg_extension(file_entry->d_name)) continue;

            PhotoEntry *e = &lib->entries[lib->count];

            // Guard: skip if combined path would not fit
            int written = snprintf(e->path, sizeof(e->path), "%s%s",
                                   subdir_path, file_entry->d_name);
            if (written < 0 || (size_t)written >= sizeof(e->path)) continue;

            snprintf(e->display, sizeof(e->display), "%s", file_entry->d_name);
            lib->count++;
        }

        closedir(sub);
    }

    closedir(root);
    return lib->count;
}
