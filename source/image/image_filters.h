#ifndef IMAGE_FILTERS_H
#define IMAGE_FILTERS_H

#include "image_buffer.h"

#define FILTER_NONE      0
#define FILTER_GRAYSCALE 1
#define FILTER_SEPIA     2
#define FILTER_WARM      3
#define FILTER_FADED     4
#define FILTER_COUNT     5

// Get human-readable filter name
const char* image_filter_name(int filter_id);

// Apply filter: src → dst. src and dst must have same dimensions.
// FILTER_NONE copies src to dst unchanged.
void image_filter_apply(int filter_id, const ImageBuffer* src, ImageBuffer* dst);

#endif // IMAGE_FILTERS_H
