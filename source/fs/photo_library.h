#ifndef PHOTO_LIBRARY_H
#define PHOTO_LIBRARY_H

#define PHOTO_DIR    "sdmc:/DCIM/"
#define MAX_PHOTOS   64

typedef struct {
    char path[256];   // full sdmc: path
    char display[64]; // filename only (for list display)
} PhotoEntry;

typedef struct {
    PhotoEntry entries[MAX_PHOTOS];
    int count;
} PhotoLibrary;

// Scan sdmc:/DCIM/ one level deep for JPEG files.
// Returns number of entries found.
int photo_library_scan(PhotoLibrary *lib);

#endif // PHOTO_LIBRARY_H
