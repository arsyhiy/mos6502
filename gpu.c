#include "gpu.h"
#include "font.h"

#define TEXT_VRAM 0x0400

void gpu_init(GPU *g)
{
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
        g->pixels[i] = 0xFF000000;
}

void gpu_tick(GPU *g, uint8_t *mem)
{
    for (int ty = 0; ty < 30; ty++) {

        for (int tx = 0; tx < 32; tx++) {

            uint8_t ch =
                mem[TEXT_VRAM + ty * 32 + tx];

            for (int row = 0; row < 8; row++) {

                uint8_t bits =
                    font8x8[ch][row];

                for (int col = 0; col < 8; col++) {

                    int px = tx * 8 + col;
                    int py = ty * 8 + row;

                    uint32_t color;

                    /* if (bits & (1 << (7 - col))) */
                    if (bits & (0x80 >> col))
                        color = 0xFFFFFFFF;
                    else
                        color = 0xFF000000;

                    g->pixels[
                        py * SCREEN_W + px
                    ] = color;
                }
            }
        }
    }
}
