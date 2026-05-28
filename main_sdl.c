#include "mos6502.h"
#include "gpu.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

#define TEXT_VRAM 0x0400

static void term_print(
    Memory *mem,
    int x,
    int y,
    const char *s)
{
    uint16_t addr =
        TEXT_VRAM + y * 32 + x;

    while (*s) {
        mem->data[addr++] = *s++;
    }
}

int main()
{
    Memory mem = {0};

    GPU gpu;
    CPU6502 cpu;

    gpu_init(&gpu);

    cpu_init(&cpu, &mem, &gpu);

    // =====================================
    // TEST TERMINAL OUTPUT
    // =====================================

    term_print(&mem, 0, 0, "MOS 6502 TERMINAL");
    term_print(&mem, 0, 2, "HELLO WORLD");
    term_print(&mem, 0, 4, "CPU + GPU + SDL");
    term_print(&mem, 0, 6, "READY.");

    // reset vector
    mem.data[0xFFFC] = 0x00;
    mem.data[0xFFFD] = 0x80;

    cpu_reset(&cpu);

    // =====================================
    // SDL
    // =====================================

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *win = SDL_CreateWindow(
        "MOS 6502 TERMINAL",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_W * 2,
        SCREEN_H * 2,
        0
    );

    SDL_Renderer *ren =
        SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    SDL_Texture *tex =
        SDL_CreateTexture(
            ren,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            SCREEN_W,
            SCREEN_H
        );

    // =====================================
    // MAIN LOOP
    // =====================================

    while (cpu.running)
    {
        SDL_Event e;

        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                cpu.running = false;
        }

        cpu_run(&cpu, 1000);

        gpu_tick(&gpu, mem.data);

        SDL_UpdateTexture(
            tex,
            NULL,
            gpu.pixels,
            SCREEN_W * sizeof(uint32_t)
        );

        SDL_RenderClear(ren);

        SDL_RenderCopy(
            ren,
            tex,
            NULL,
            NULL
        );

        SDL_RenderPresent(ren);

        SDL_Delay(16);
    }

    // =====================================
    // CLEANUP
    // =====================================

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);

    SDL_Quit();

    return 0;
}
