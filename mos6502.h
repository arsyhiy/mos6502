#ifndef MOS6502_H
#define MOS6502_H

#include <stdint.h>
#include <stdbool.h>

#define MEM_SIZE 0x10000

#define SCREEN_W 256
#define SCREEN_H 240

// =========================
// FLAGS
// =========================
typedef enum {
    FLAG_C = 0,
    FLAG_Z,
    FLAG_I,
    FLAG_D,
    FLAG_B,
    FLAG_U,
    FLAG_V,
    FLAG_N
} Flag;

// =========================
// ADDRESS MODES
// =========================
typedef enum {
    IMM, ZP, ABS, IND,
    ZPX, ZPY, ABX, ABY,
    IMP, IZX, IZY, REL, ACC
} AddrMode;

// =========================
// MEMORY
// =========================
typedef struct {
    uint8_t data[MEM_SIZE];
} Memory;

typedef struct CPU6502 CPU6502;
typedef struct GPU GPU;   // 👈 forward declaration (OK)

// =========================
// OPERAND
// =========================
typedef struct {
    uint16_t addr;
    uint8_t value;
} Operand;

// =========================
// INSTRUCTION FUNCTION
// =========================
typedef void (*InstrFn)(CPU6502 *, Operand);

// =========================
// OPCODE
// =========================
typedef struct {
    InstrFn fn;
    AddrMode mode;
    uint8_t cycles;
} Opcode;

// =========================
// CPU
// =========================
typedef struct CPU6502 {
    uint8_t a, x, y;
    uint8_t sp;
    uint8_t status;
    uint16_t pc;

    GPU *gpu;

    uint64_t cycles;
    bool running;

    Memory *mem;
    Opcode op[256];

    uint8_t cycles_left;
} CPU6502;

// =========================
// API
// =========================
void cpu_init(CPU6502 *cpu, Memory *mem, GPU *gpu);
void cpu_reset(CPU6502 *cpu);
void cpu_run(CPU6502 *cpu, int cycles);

#endif
