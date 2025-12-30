#include "moving_average.h"
#include <string.h>

void ma_init(moving_average_t *ma, int size) {
    if (size > 16) size = 16;
    ma->size = size;
    ma->index = 0;
    ma->count = 0;
    memset(ma->buffer, 0, sizeof(ma->buffer));
}

void ma_add(moving_average_t *ma, float value) {
    ma->buffer[ma->index] = value;
    ma->index = (ma->index + 1) % ma->size;
    if (ma->count < ma->size) ma->count++;
}

float ma_get(moving_average_t *ma) {
    if (ma->count == 0) return 0.0f;
    float sum = 0.0f;
    for (int i = 0; i < ma->count; i++) sum += ma->buffer[i];
    return sum / ma->count;
}
