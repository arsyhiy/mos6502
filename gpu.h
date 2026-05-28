#ifndef GPU_H
#define GPU_H

#include <stdint.h>
#include <string.h>

#define SCREEN_W 256
#define SCREEN_H 240

typedef struct GPU {
    uint32_t pixels[SCREEN_W * SCREEN_H];
} GPU;

void gpu_init(GPU *g);
void gpu_tick(GPU *g, uint8_t *mem);

#endif
