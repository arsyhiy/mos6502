#ifndef EMULATOR_H
#define EMULATOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================
   MEMORY
========================= */

typedef struct {
    uint8_t data[0x10000];
} Memory;

/* =========================
   CPU
========================= */

typedef struct CPU6502 {

    Memory* mem;

    uint8_t a;
    uint8_t x;
    uint8_t y;

    uint16_t pc;
    uint8_t sp;

    uint8_t status;

    bool running;
    uint64_t cycles;

    void (*op[256])(struct CPU6502*);

} CPU6502;

/* =========================
   MEMORY API
========================= */

void mem_write(Memory* mem, uint16_t addr, uint8_t value);

uint8_t mem_read(
    Memory* mem,
    uint16_t addr
);

void mem_load(
    Memory* mem,
    uint16_t addr,
    const uint8_t* blob,
    size_t size
);

/* =========================
   CPU LIFECYCLE
========================= */

CPU6502* cpu_create(Memory* mem);

void cpu_destroy(CPU6502* cpu);

void cpu_reset(CPU6502* cpu);

void cpu_run(CPU6502* cpu);

void cpu_step(CPU6502* cpu);

/* =========================
   INTERNAL INIT
========================= */

void init_opcodes(CPU6502* cpu);

#ifdef __cplusplus
}
#endif

#endif
