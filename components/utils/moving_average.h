#ifndef MOVING_AVERAGE_H
#define MOVING_AVERAGE_H

#include <stdint.h>

typedef struct {
    float buffer[16];
    int size;
    int index;
    int count;
} moving_average_t;

void ma_init(moving_average_t *ma, int size);
void ma_add(moving_average_t *ma, float value);
float ma_get(moving_average_t *ma);

#endif
