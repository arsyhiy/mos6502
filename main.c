#include "mos6502.h"
#include <SDL2/SDL.h>
#include <stdio.h>

int main()
{
    Memory mem = {0};
    GPU gpu = {0};
    CPU6502 cpu;

    uint8_t program[] = {
        0xA9, 0x01,
        0x85, 0x10,
        0xA9, 0x02,
        0x85, 0x11,
        0xA5, 0x10,
        0x65, 0x11,
        0x8D, 0x00, 0x20, // write to "screen"
        0x00
    };

    for (int i = 0; i < sizeof(program); i++)
        mem.data[0x8000 + i] = program[i];

    mem.data[0xFFFC] = 0x00;
    mem.data[0xFFFD] = 0x80;

    cpu_init(&cpu, &mem, &gpu);
    cpu_reset(&cpu);

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *w = SDL_CreateWindow("6502",
        100, 100, SCREEN_W, SCREEN_H, 0);

    SDL_Renderer *r = SDL_CreateRenderer(w, -1, 0);
    SDL_Texture *t = SDL_CreateTexture(
        r,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_W, SCREEN_H
    );

    while (cpu.running)
    {
        cpu_run(&cpu, 1000);

        SDL_UpdateTexture(t, NULL, gpu.pixels, SCREEN_W * 4);
        SDL_RenderClear(r);
        SDL_RenderCopy(r, t, NULL, NULL);
        SDL_RenderPresent(r);

        SDL_Event e;
        while (SDL_PollEvent(&e))
            if (e.type == SDL_QUIT)
                cpu.running = false;
    }

    return 0;
}
