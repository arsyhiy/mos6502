/* mos6502.c: a library implementing a fully functional MOS 6502 CPU in C */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEM_SIZE 0x10000 // 64 KB of addressable memory

// flags

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

// addresing modes

typedef enum {
  IMM,
  ZP,
  ABS,
  IND,
  ZPX,
  ZPY,
  ABX,
  ABY,
  IMP,
  IZX,
  IZY,
  REL,
  ACC
} AddrMode;

// memory
typedef struct {
  uint8_t data[MEM_SIZE];
} Memory;

typedef struct CPU6502 CPU6502;

typedef void (*InstrFn)(CPU6502 *, AddrMode);

// opcode
typedef struct {
  InstrFn fn;
  AddrMode mode;
} Opcode;

// cpu
struct CPU6502 {
  uint8_t a, x, y;
  uint8_t sp;
  uint8_t status;
  uint16_t pc;

  uint64_t cycles;
  bool running;

  Memory *mem;
  Opcode op[256];


  uint8_t cycles_left;
};

/* // ========================= */
/* // MEMORY ACCESS */
/* // ========================= */

static uint8_t mem_read(Memory *mem, uint16_t addr) { return mem->data[addr]; }

static void mem_write(Memory *mem, uint16_t addr, uint8_t v) {
  mem->data[addr] = v;
}

// =========================
// FLAGS
// =========================

static void set_flag(CPU6502 *cpu, Flag f, bool v) {
  if (v)
    cpu->status |= (1 << f);
  else
    cpu->status &= ~(1 << f);
}

static void update_zn(CPU6502 *cpu, uint8_t v) {
  set_flag(cpu, FLAG_Z, v == 0);
  set_flag(cpu, FLAG_N, v & 0x80);
}

// =========================
// STACK
// =========================

static void push(CPU6502 *cpu, uint8_t v) {
  mem_write(cpu->mem, 0x100 + cpu->sp, v);
  cpu->sp--;
}

static uint8_t pop(CPU6502 *cpu) {
  cpu->sp++;
  return mem_read(cpu->mem, 0x100 + cpu->sp);
}

// =========================
// FETCH
// =========================

static uint8_t fetch(CPU6502 *cpu) { return mem_read(cpu->mem, cpu->pc++); }

static uint16_t fetch_word(CPU6502 *cpu) {
  uint8_t lo = fetch(cpu);
  uint8_t hi = fetch(cpu);
  return (hi << 8) | lo;
}

// =========================
// ADDRESSING
// =========================

static uint16_t get_addr(CPU6502 *cpu, AddrMode mode) {
  switch (mode) {

  case IMM:
    return cpu->pc++;

  case ZP:
    return fetch(cpu);

  case ABS:
    return fetch_word(cpu);

  case ZPX:
    return (fetch(cpu) + cpu->x) & 0xFF;

  case ZPY:
    return (fetch(cpu) + cpu->y) & 0xFF;

  case ABX:
    return fetch_word(cpu) + cpu->x;

  case ABY:
    return fetch_word(cpu) + cpu->y;

  case IND: {
    uint16_t ptr = fetch_word(cpu);
    uint8_t lo = mem_read(cpu->mem, ptr);
    uint8_t hi = mem_read(cpu->mem, (ptr & 0xFF00) | ((ptr + 1) & 0xFF));
    return (hi << 8) | lo;
  }

  case IZX: {
    uint8_t zp = fetch(cpu);
    uint8_t ptr = (zp + cpu->x) & 0xFF;
    uint8_t lo = mem_read(cpu->mem, ptr);
    uint8_t hi = mem_read(cpu->mem, (ptr + 1) & 0xFF);
    return (hi << 8) | lo;
  }

  case IZY: {
    uint8_t zp = fetch(cpu);
    uint8_t lo = mem_read(cpu->mem, zp);
    uint8_t hi = mem_read(cpu->mem, (zp + 1) & 0xFF);
    return ((hi << 8) | lo) + cpu->y;
  }

  case REL: {
    int8_t off = (int8_t)fetch(cpu);
    return cpu->pc + off;
  }

  case ACC:
  case IMP:
  default:
    return 0;
  }
}

// =========================
// LOAD / STORE
// =========================

static void LDA(CPU6502 *c, AddrMode m) {
  c->a = mem_read(c->mem, get_addr(c, m));
  update_zn(c, c->a);
}

static void LDX(CPU6502 *c, AddrMode m) {
  c->x = mem_read(c->mem, get_addr(c, m));
  update_zn(c, c->x);
}

static void LDY(CPU6502 *c, AddrMode m) {
  c->y = mem_read(c->mem, get_addr(c, m));
  update_zn(c, c->y);
}

static void STA(CPU6502 *c, AddrMode m) {
  mem_write(c->mem, get_addr(c, m), c->a);
}

// =========================
// TRANSFERS
// =========================

static void TAX(CPU6502 *c, AddrMode m) {
  (void)m;
  c->x = c->a;
  update_zn(c, c->x);
}
static void TAY(CPU6502 *c, AddrMode m) {
  (void)m;
  c->y = c->a;
  update_zn(c, c->y);
}
static void TXA(CPU6502 *c, AddrMode m) {
  (void)m;
  c->a = c->x;
  update_zn(c, c->a);
}
static void TYA(CPU6502 *c, AddrMode m) {
  (void)m;
  c->a = c->y;
  update_zn(c, c->a);
}

// =========================
// ARITHMETIC
// =========================

static void ADC(CPU6502 *c, AddrMode m) {
  uint8_t v = mem_read(c->mem, get_addr(c, m));
  uint16_t r = c->a + v + (c->status & (1 << FLAG_C));

  set_flag(c, FLAG_C, r > 0xFF);
  c->a = r & 0xFF;
  update_zn(c, c->a);
}

// =========================
// BRANCH
// =========================

static void branch(CPU6502 *c, bool cond) {
  int8_t off = (int8_t)fetch(c);
  if (cond)
    c->pc += off;
}

static void BEQ(CPU6502 *c, AddrMode m) {
  (void)m;
  branch(c, c->status & (1 << FLAG_Z));
}
static void BNE(CPU6502 *c, AddrMode m) {
  (void)m;
  branch(c, !(c->status & (1 << FLAG_Z)));
}

// =========================
// JUMP
// =========================

static void JMP(CPU6502 *c, AddrMode m) { c->pc = get_addr(c, m); }

// =========================
// SYSTEM
// =========================

static void NOP(CPU6502 *c, AddrMode m) {
  (void)c;
  (void)m;
}

static void BRK(CPU6502 *c, AddrMode m) {
  (void)m;
  c->running = false;
}

// there will go all instructions need to rewrite that's why they will be
// commented

//   there  i will describe all instructions
// */
//
// /* void lda_imm( */
// /*     CPU6502* cpu */
// /* ) { */
//
// /*     cpu->a = */
// /*         imm(cpu); */
//
// /*     set_flag( */
// /*         cpu, */
// /*         FLAG_Z, */
// /*         cpu->a == 0 */
// /*     ); */
//
// /*     set_flag( */
// /*         cpu, */
// /*         FLAG_N, */
// /*         cpu->a & 0x80 */
// /*     ); */
// /* } */
//
// /* void sta_abs( */
// /*     CPU6502* cpu */
// /* ) { */
//
// /*     cpu_write( */
// /*         cpu, */ /*         abs_addr(cpu), */ /*         cpu->a */
// /*     ); */
// /* } */
//
// /* void jmp_abs( */
// /*     CPU6502* cpu */
// /* ) { */
//
// /*     cpu->pc = */
// /*         abs_addr(cpu); */
// /* } */
//
// /* void jsr_abs( */
// /*     CPU6502* cpu */
// /* ) { */
//
// /*     uint16_t addr = */
// /*         abs_addr(cpu); */
//
// /*     uint16_t ret = */
// /*         ( */
// /*             cpu->pc */
// /*             - */
// /*             1 */
// /*         ) */
// /*         & */
// /*         0xFFFF; */
//
// /*     push( */
// /*         cpu, */
// /*         ( */
// /*             ret */
// /*             >> */
// /*             8 */
// /*         ) */
// /*     ); */
//
// /*     push( */
// /*         cpu, */
// /*         ret */
// /*     ); */
//
// /*     cpu->pc = */
// /*         addr; */
// /* } */
//
// /* void rts_imp( */
// /*     CPU6502* cpu */
// /* ) { */
//
// /*     uint8_t lo = */
// /*         pop(cpu); */
//
// /*     uint8_t hi = */
// /*         pop(cpu); */
//
// /*     cpu->pc = */
// /*         ( */
// /*             ( */
// /*                 hi */
// /*                 << */
// /*                 8 */
// /*             ) */
// /*             | */
// /*             lo */
// /*         ) */
// /*         + */
// /*         1; */
// /* } */
//
// /* void brk_imp( */
// /*     CPU6502* cpu */
// /* ) { */
//
// /*     cpu->running = */
// /*         false; */
//
// /*     cpu->pc++; */
// /* } */
//
// /* void clc_imp( */
// /*     CPU6502* cpu */
// /* ) { */
//
// /*     set_flag( */
// /*         cpu, */
// /*         FLAG_C, */
// /*         false */
// /*     ); */
// /* } */
//
// /* void adc_imm( */
// /*     CPU6502* cpu */
// /* ) { */
//
// /*     uint8_t value = */
// /*         imm(cpu); */
//
// /*     uint8_t carry = */
// /*         cpu->status */
// /*         & */
// /*         1; */
//
// /*     uint16_t r = */
// /*         cpu->a */
// /*         + */
// /*         value */
// /*         + */
// /*         carry; */
//
// /*     set_flag( */
// /*         cpu, */
// /*         FLAG_C, */
// /*         r > 0xFF */
// /*     ); */
//
// /*     cpu->a = */
// /*         r */
// /*         & */
// /*         0xFF; */
//
// /*     set_flag( */
// /*         cpu, */
// /*         FLAG_Z, */
// /*         cpu->a == 0 */
// /*     ); */
//
// /*     set_flag( */
// /*         cpu, */
// /*         FLAG_N, */
// /*         cpu->a & 0x80 */
// /*     ); */
// /* } */
//
//
//
// // =========================
// // Transfer Instructions
// // =========================
//
// static void update_zn(
//     CPU6502* cpu,
//     uint8_t value
// ){
//     set_flag(
//         cpu,
//         Z,
//         value == 0
//     );
//
//     set_flag(
//         cpu,
//         N,
//         value & 0x80
//     );
// }
//
//
// // Load Accumulator
//
// void LDA(
//     CPU6502* cpu,
//     uint16_t addr
// ){
//     cpu->a =
//         mem_read(
//             cpu->mem,
//             addr
//         );
//
//     update_zn(
//         cpu,
//         cpu->a
//     );
// }
//
//
// // Load X
//
// void LDX(
//     CPU6502* cpu,
//     uint16_t addr
// ){
//     cpu->x =
//         mem_read(
//             cpu->mem,
//             addr
//         );
//
//     update_zn(
//         cpu,
//         cpu->x
//     );
// }
//
//
// // Load Y
//
// void LDY(
//     CPU6502* cpu,
//     uint16_t addr
// ){
//     cpu->y =
//         mem_read(
//             cpu->mem,
//             addr
//         );
//
//     update_zn(
//         cpu,
//         cpu->y
//     );
// }
//
//
// // Store Accumulator
//
// void STA(
//     CPU6502* cpu,
//     uint16_t addr
// ){
//     mem_write(
//         cpu->mem,
//         addr,
//         cpu->a
//     );
// }
//
//
// // Store X
//
// void STX(
//     CPU6502* cpu,
//     uint16_t addr
// ){
//     mem_write(
//         cpu->mem,
//         addr,
//         cpu->x
//     );
// }
//
//
// // Store Y
//
// void STY(
//     CPU6502* cpu,
//     uint16_t addr
// ){
//     mem_write(
//         cpu->mem,
//         addr,
//         cpu->y
//     );
// }
//
//
// // Transfer A → X
//
// void TAX(
//     CPU6502* cpu
// ){
//     cpu->x =
//         cpu->a;
//
//     update_zn(
//         cpu,
//         cpu->x
//     );
// }
//
//
// // Transfer A → Y
//
// void TAY(
//     CPU6502* cpu
// ){
//     cpu->y =
//         cpu->a;
//
//     update_zn(
//         cpu,
//         cpu->y
//     );
// }
//
//
// // Transfer SP → X
//
// void TSX(
//     CPU6502* cpu
// ){
//     cpu->x =
//         cpu->sp;
//
//     update_zn(
//         cpu,
//         cpu->x
//     );
// }
//
//
// // Transfer X → A
//
// void TXA(
//     CPU6502* cpu
// ){
//     cpu->a =
//         cpu->x;
//
//     update_zn(
//         cpu,
//         cpu->a
//     );
// }
//
//
// // Transfer X → SP
//
// void TXS(
//     CPU6502* cpu
// ){
//     cpu->sp =
//         cpu->x;
// }
//
//
// // Transfer Y → A
//
// void TYA(
//     CPU6502* cpu
// ){
//     cpu->a =
//         cpu->y;
//
//     update_zn(
//         cpu,
//         cpu->a
//     );
// }
//
// // =========================
// // Stack Instructions
// // =========================
//
// /* static void update_zn( */
// /*     CPU6502* cpu, */
// /*     uint8_t value */
// /* ){ */
// /*     set_flag( */
// /*         cpu, */
// /*         Z, */
// /*         value == 0 */
// /*     ); */
//
// /*     set_flag( */
// /*         cpu, */
// /*         N, */
// /*         value & 0x80 */
// /*     ); */
// /* } */
//
//
// // Push Accumulator
//
// void PHA(
//     CPU6502* cpu
// ){
//     push(
//         cpu,
//         cpu->a
//     );
// }
//
//
// // Push Processor Status
//
// void PHP(
//     CPU6502* cpu
// ){
//     push(
//         cpu,
//         cpu->status
//         |
//         (1 << B)
//         |
//         (1 << U)
//     );
// }
//
//
// // Pull Accumulator
//
// void PLA(
//     CPU6502* cpu
// ){
//     cpu->a =
//         pop(
//             cpu
//         );
//
//     update_zn(
//         cpu,
//         cpu->a
//     );
// }
//
//
// // Pull Processor Status
//
// void PLP(
//     CPU6502* cpu
// ){
//     cpu->status =
//         pop(
//             cpu
//         );
//
//     cpu->status |=
//         (1 << U);
// }
//
//
// // =========================
// // Decrements & Increments
// // =========================
//
// /* static void update_zn( */
// /*     CPU6502* cpu, */
// /*     uint8_t value */
// /* ){ */
// /*     set_flag( */
// /*         cpu, */
// /*         Z, */
// /*         value == 0 */
// /*     ); */
//
// /*     set_flag( */
// /*         cpu, */
// /*         N, */
// /*         value & 0x80 */
// /*     ); */
// /* } */
//
//
// // Decrement Memory
//
// void DEC(
//     CPU6502* cpu,
//     uint16_t addr
// ){
//     uint8_t value =
//         mem_read(
//             cpu->mem,
//             addr
//         );
//
//     value--;
//
//     mem_write(
//         cpu->mem,
//         addr,
//         value
//     );
//
//     update_zn(
//         cpu,
//         value
//     );
// }
//
//
// // Decrement X
//
// void DEX(
//     CPU6502* cpu
// ){
//     cpu->x--;
//
//     update_zn(
//         cpu,
//         cpu->x
//     );
// }
//
//
// // Decrement Y
//
// void DEY(
//     CPU6502* cpu
// ){
//     cpu->y--;
//
//     update_zn(
//         cpu,
//         cpu->y
//     );
// }
//
//
// // Increment Memory
//
// void INC(
//     CPU6502* cpu,
//     uint16_t addr
// ){
//     uint8_t value =
//         mem_read(
//             cpu->mem,
//             addr
//         );
//
//     value++;
//
//     mem_write(
//         cpu->mem,
//         addr,
//         value
//     );
//
//     update_zn(
//         cpu,
//         value
//     );
// }
//
//
// // Increment X
//
// void INX(
//     CPU6502* cpu
// ){
//     cpu->x++;
//
//     update_zn(
//         cpu,
//         cpu->x
//     );
// }
//
//
// // Increment Y
//
// void INY(
//     CPU6502* cpu
// ){
//     cpu->y++;
//
//     update_zn(
//         cpu,
//         cpu->y
//     );
// }
//
//
//
// // =========================
// // Arithmetic Operations
// // =========================
//
// /* static void update_zn( */
// /*     CPU6502* cpu, */
// /*     uint8_t value */
// /* ){ */
// /*     set_flag( */
// /*         cpu, */
// /*         Z, */
// /*         value == 0 */
// /*     ); */
//
// /*     set_flag( */
// /*         cpu, */
// /*         N, */
// /*         value & 0x80 */
// /*     ); */
// /* } */
//
//
// // Add With Carry
//
// void ADC(
//     CPU6502* cpu,
//     uint16_t addr
// ){
//
//     uint8_t value =
//         mem_read(
//             cpu->mem,
//             addr
//         );
//
//     uint8_t carry =
//         (
//             cpu->status
//             &
//             (1 << C)
//         )
//         ? 1
//         : 0;
//
//     uint16_t result =
//         cpu->a
//         +
//         value
//         +
//         carry;
//
//     set_flag(
//         cpu,
//         C,
//         result > 0xFF
//     );
//
//     set_flag(
//         cpu,
//         V,
//         (
//             ~(
//                 cpu->a
//                 ^
//                 value
//             )
//             &
//             (
//                 cpu->a
//                 ^
//                 result
//             )
//             &
//             0x80
//         )
//     );
//
//     cpu->a =
//         result
//         &
//         0xFF;
//
//     update_zn(
//         cpu,
//         cpu->a
//     );
// }
//
//
// /* void ADC( */
// /*     CPU6502* cpu, */
// /*     uint16_t addr */
// /* ){ */
// /*     uint8_t value = */
// /*         mem_read( */
// /*             cpu->mem, */
// /*             addr */
// /*         ); */
//
// /*     uint8_t carry = */
// /*         (cpu->status & (1 << FLAG_C)) */
// /*         ? 1 */
// /*         : 0; */
//
// /*     // ========================= */
// /*     // DECIMAL MODE (BCD) */
// /*     // ========================= */
// /*     if (cpu->status & (1 << FLAG_D)) */
// /*     { */
// /*         uint8_t lo = */
// /*             (cpu->a & 0x0F) */
// /*             + (value & 0x0F) */
// /*             + carry; */
//
// /*         uint8_t hi = */
// /*             (cpu->a >> 4) */
// /*             + (value >> 4); */
//
// /*         if (lo > 9) */
// /*         { */
// /*             lo -= 10; */
// /*             hi++; */
// /*         } */
//
// /*         set_flag(cpu, FLAG_C, hi > 9); */
//
// /*         cpu->a = */
// /*             ((hi % 10) << 4) */
// /*             | (lo & 0x0F); */
//
// /*         update_zn(cpu, cpu->a); */
// /*         return; */
// /*     } */
//
// /*     // ========================= */
// /*     // BINARY MODE (NORMAL 6502) */
// /*     // ========================= */
// /*     uint16_t result = */
// /*         cpu->a */
// /*         + value */
// /*         + carry; */
//
// /*     set_flag( */
// /*         cpu, */
// /*         FLAG_C, */
// /*         result > 0xFF */
// /*     ); */
//
// /*     set_flag( */
// /*         cpu, */
// /*         FLAG_V, */
// /*         (~(cpu->a ^ value) & (cpu->a ^ result) & 0x80) */
// /*     ); */
//
// /*     cpu->a = result & 0xFF; */
//
// /*     update_zn(cpu, cpu->a); */
// /* } */
//
//
// // Subtract With Carry
//
// void SBC(
//     CPU6502* cpu,
//     uint16_t addr
// ){
//
//     uint8_t value =
//         mem_read(
//             cpu->mem,
//             addr
//         );
//
//     value ^= 0xFF;
//
//     uint8_t carry =
//         (
//             cpu->status
//             &
//             (1 << C)
//         )
//         ? 1
//         : 0;
//
//     uint16_t result =
//         cpu->a
//         +
//         value
//         +
//         carry;
//
//     set_flag(
//         cpu,
//         C,
//         result > 0xFF
//     );
//
//     set_flag(
//         cpu,
//         V,
//         (
//             ~(
//                 cpu->a
//                 ^
//                 value
//             )
//             &
//             (
//                 cpu->a
//                 ^
//                 result
//             )
//             &
//             0x80
//         )
//     );
//
//     cpu->a =
//         result
//         &
//         0xFF;
//
//     update_zn(
//         cpu,
//         cpu->a
//     );
// }
//
//
// // =========================
// // Logical Operations
// // =========================
//
// /* static void update_zn( */
// /*     CPU6502* cpu, */
// /*     uint8_t value */
// /* ){ */
// /*     set_flag( */
// /*         cpu, */
// /*         Z, */
// /*         value == 0 */
// /*     ); */
//
// /*     set_flag( */
// /*         cpu, */
// /*         N, */
// /*         value & 0x80 */
// /*     ); */
// /* } */
//
//
// // AND
// // A = A & M
//
// void AND(
//     CPU6502* cpu,
//     uint16_t addr
// ){
//
//     cpu->a &=
//         mem_read(
//             cpu->mem,
//             addr
//         );
//
//     update_zn(
//         cpu,
//         cpu->a
//     );
// }
//
//
// // EOR (XOR)
// // A = A ^ M
//
// void EOR(
//     CPU6502* cpu,
//     uint16_t addr
// ){
//
//     cpu->a ^=
//         mem_read(
//             cpu->mem,
//             addr
//         );
//
//     update_zn(
//         cpu,
//         cpu->a
//     );
// }
//
//
// // ORA (OR)
// // A = A | M
//
// void ORA(
//     CPU6502* cpu,
//     uint16_t addr
// ){
//
//     cpu->a |=
//         mem_read(
//             cpu->mem,
//             addr
//         );
//
//     update_zn(
//         cpu,
//         cpu->a
//     );
// }
//
//
//
// /* static void update_zn(CPU6502* cpu, uint8_t v) */
// /* { */
// /*     set_flag(cpu, Z, v == 0); */
// /*     set_flag(cpu, N, v & 0x80); */
// /* } */
//
// // shift & rotate instructions
// void ASL(CPU6502* cpu, uint16_t addr)
// {
//     uint8_t value = mem_read(cpu->mem, addr);
//
//     set_flag(cpu, C, value & 0x80);
//
//     value <<= 1;
//
//     mem_write(cpu->mem, addr, value);
//
//     update_zn(cpu, value);
// }
//
//
// /* void ASL(CPU6502* cpu, uint16_t addr) */
// /* { */
// /*     uint8_t value; */
//
// /*     if (addr == 0) // ACC mode */
// /*         value = cpu->a; */
// /*     else */
// /*         value = read_mem(cpu, addr); */
//
// /*     set_flag(cpu, FLAG_C, value & 0x80); */
//
// /*     value <<= 1; */
//
// /*     if (addr == 0) */
// /*         cpu->a = value; */
// /*     else */
// /*         write_mem(cpu, addr, value); */
//
// /*     update_zn(cpu, value); */
// /* } */
//
//
// void ASL_A(CPU6502* cpu)
// {
//     set_flag(cpu, C, cpu->a & 0x80);
//
//     cpu->a <<= 1;
//
//     update_zn(cpu, cpu->a);
// }
//
// void LSR(CPU6502* cpu, uint16_t addr)
// {
//     uint8_t value = mem_read(cpu->mem, addr);
//
//     set_flag(cpu, C, value & 0x01);
//
//     value >>= 1;
//
//     mem_write(cpu->mem, addr, value);
//
//     update_zn(cpu, value);
// }
//
// void LSR_A(CPU6502* cpu)
// {
//     set_flag(cpu, C, cpu->a & 0x01);
//
//     cpu->a >>= 1;
//
//     update_zn(cpu, cpu->a);
// }
//
// void ROL(CPU6502* cpu, uint16_t addr)
// {
//     uint8_t value = mem_read(cpu->mem, addr);
//
//     uint8_t old_c = (cpu->status & (1 << C)) ? 1 : 0;
//
//     set_flag(cpu, C, value & 0x80);
//
//     value = (value << 1) | old_c;
//
//     mem_write(cpu->mem, addr, value);
//
//     update_zn(cpu, value);
// }
//
// void ROL_A(CPU6502* cpu)
// {
//     uint8_t old_c = (cpu->status & (1 << C)) ? 1 : 0;
//
//     set_flag(cpu, C, cpu->a & 0x80);
//
//     cpu->a = (cpu->a << 1) | old_c;
//
//     update_zn(cpu, cpu->a);
// }
//
// void ROR(CPU6502* cpu, uint16_t addr)
// {
//     uint8_t value = mem_read(cpu->mem, addr);
//
//     uint8_t old_c = (cpu->status & (1 << C)) ? 1 : 0;
//
//     set_flag(cpu, C, value & 0x01);
//
//     value = (value >> 1) | (old_c << 7);
//
//     mem_write(cpu->mem, addr, value);
//
//     update_zn(cpu, value);
// }
//
// void ROR_A(CPU6502* cpu)
// {
//     uint8_t old_c = (cpu->status & (1 << C)) ? 1 : 0;
//
//     set_flag(cpu, C, cpu->a & 0x01);
//
//     cpu->a = (cpu->a >> 1) | (old_c << 7);
//
//     update_zn(cpu, cpu->a);
// }
//
// // =========================
// // Flag Instructions
// // =========================
//
// void CLC(CPU6502* cpu)
// {
//     set_flag(cpu, C, false);
// }
//
// void CLD(CPU6502* cpu)
// {
//     set_flag(cpu, D, false);
// }
//
// void CLI(CPU6502* cpu)
// {
//     set_flag(cpu, I, false);
// }
//
// void CLV(CPU6502* cpu)
// {
//     set_flag(cpu, V, false);
// }
//
// void SEC(CPU6502* cpu)
// {
//     set_flag(cpu, C, true);
// }
//
// void SED(CPU6502* cpu)
// {
//     set_flag(cpu, D, true);
// }
//
// void SEI(CPU6502* cpu)
// {
//     set_flag(cpu, I, true);
// }
//
//
// // =========================
// // Comparison Instructions
// // =========================
//
// static void update_cmp_flags(
//     CPU6502* cpu,
//     uint8_t reg,
//     uint8_t value
// ){
//     uint16_t result =
//         (uint16_t)reg - value;
//
//     set_flag(cpu, C, reg >= value);
//     set_flag(cpu, Z, reg == value);
//     set_flag(cpu, N, result & 0x80);
// }
//
//
// // Compare A
//
// void CMP(
//     CPU6502* cpu,
//     uint16_t addr
// ){
//     uint8_t value =
//         mem_read(cpu->mem, addr);
//
//     update_cmp_flags(
//         cpu,
//         cpu->a,
//         value
//     );
// }
//
//
// // Compare X
//
// void CPX(
//     CPU6502* cpu,
//     uint16_t addr
// ){
//     uint8_t value =
//         mem_read(cpu->mem, addr);
//
//     update_cmp_flags(
//         cpu,
//         cpu->x,
//         value
//     );
// }
//
//
// // Compare Y
//
// void CPY(
//     CPU6502* cpu,
//     uint16_t addr
// ){
//     uint8_t value =
//         mem_read(cpu->mem, addr);
//
//     update_cmp_flags(
//         cpu,
//         cpu->y,
//         value
//     );
// }
//
// // =========================
// // BIT Instruction
// // =========================
//
// void BIT(
//     CPU6502* cpu,
//     uint16_t addr
// ){
//     uint8_t value =
//         mem_read(cpu->mem, addr);
//
//     uint8_t result =
//         cpu->a & value;
//
//     // Z flag: result == 0
//     set_flag(cpu, Z, result == 0);
//
//     // N flag: bit 7 of memory
//     set_flag(cpu, N, value & 0x80);
//
//     // V flag: bit 6 of memory
//     set_flag(cpu, V, value & 0x40);
// }
//
// // conditional branch instructions
//
// static void branch(
//     CPU6502* cpu,
//     bool condition
// ){
//     int8_t offset =
//         (int8_t)mem_read(
//             cpu->mem,
//             cpu->pc++
//         );
//
//     if (condition)
//     {
//         cpu->pc =
//             cpu->pc + offset;
//     }
// }
//
//
// void IRQ(CPU6502* cpu)
// {
//     if (cpu->status & (1 << FLAG_I))
//         return;
//
//     push(cpu, (cpu->pc >> 8) & 0xFF);
//     push(cpu, cpu->pc & 0xFF);
//
//     push(cpu, cpu->status);
//
//     set_flag(cpu, FLAG_I, true);
//
//     uint16_t lo = read_mem(cpu, 0xFFFE);
//     uint16_t hi = read_mem(cpu, 0xFFFF);
//
//     cpu->pc = (hi << 8) | lo;
// }
//
//
// void NMI(CPU6502* cpu)
// {
//     push(cpu, (cpu->pc >> 8) & 0xFF);
//     push(cpu, cpu->pc & 0xFF);
//
//     push(cpu, cpu->status);
//
//     set_flag(cpu, FLAG_I, true);
//
//     uint16_t lo = read_mem(cpu, 0xFFFA);
//     uint16_t hi = read_mem(cpu, 0xFFFB);
//
//     cpu->pc = (hi << 8) | lo;
// }
//
// /* static void branch(CPU6502* cpu, bool condition) */
// /* { */
// /*     int8_t offset = (int8_t)read_mem(cpu, cpu->pc++); */
//
// /*     if (condition) */
// /*     { */
// /*         uint16_t old_pc = cpu->pc; */
// /*         cpu->pc = old_pc + offset; */
// /*     } */
// /* } */
//
//
// // =========================
// // Conditional Branches
// // =========================
//
// void BCC(CPU6502* cpu)
// {
//     branch(
//         cpu,
//         !(cpu->status & (1 << C))
//     );
// }
//
// void BCS(CPU6502* cpu)
// {
//     branch(
//         cpu,
//         (cpu->status & (1 << C))
//     );
// }
//
// void BEQ(CPU6502* cpu)
// {
//     branch(
//         cpu,
//         (cpu->status & (1 << Z))
//     );
// }
//
// void BMI(CPU6502* cpu)
// {
//     branch(
//         cpu,
//         (cpu->status & (1 << N))
//     );
// }
//
// void BNE(CPU6502* cpu)
// {
//     branch(
//         cpu,
//         !(cpu->status & (1 << Z))
//     );
// }
//
// void BPL(CPU6502* cpu)
// {
//     branch(
//         cpu,
//         !(cpu->status & (1 << N))
//     );
// }
//
// void BVC(CPU6502* cpu)
// {
//     branch(
//         cpu,
//         !(cpu->status & (1 << V))
//     );
// }
//
// void BVS(CPU6502* cpu)
// {
//     branch(
//         cpu,
//         (cpu->status & (1 << V))
//     );
// }
//
//
// // =========================
// // Jumps & Subroutines
// // =========================
//
//
// // JMP (absolute)
//
// void JMP(
//     CPU6502* cpu,
//     uint16_t addr
// ){
//     cpu->pc = addr;
// }
//
//
// // JSR (Jump to Subroutine)
//
// void JSR(
//     CPU6502* cpu,
//     uint16_t addr
// ){
//     uint16_t return_addr =
//         cpu->pc - 1;
//
//     // push high byte
//     push(cpu, (return_addr >> 8) & 0xFF);
//
//     // push low byte
//     push(cpu, return_addr & 0xFF);
//
//     cpu->pc = addr;
// }
//
//
// // RTS (Return from Subroutine)
//
// void RTS(
//     CPU6502* cpu
// ){
//     uint8_t lo = pop(cpu);
//     uint8_t hi = pop(cpu);
//
//     cpu->pc =
//         ((hi << 8) | lo)
//         + 1;
// }
// // =========================
// // Interrupts
// // =========================
//
// void BRK(
//     CPU6502* cpu
// ){
//     cpu->pc++;
//
//     // push PC high
//     push(cpu, (cpu->pc >> 8) & 0xFF);
//
//     // push PC low
//     push(cpu, cpu->pc & 0xFF);
//
//     // push status with B + U set
//     push(
//         cpu,
//         cpu->status
//         | (1 << B)
//         | (1 << U)
//     );
//
//     // set interrupt disable
//     set_flag(cpu, I, true);
//
//     // load interrupt vector
//     uint16_t lo =
//         mem_read(cpu->mem, 0xFFFE);
//
//     uint16_t hi =
//         mem_read(cpu->mem, 0xFFFF);
//
//     cpu->pc =
//         (hi << 8) | lo;
// }
//
// // =========================
// // Other Instructions
// // =========================
//
// void NOP(
//     CPU6502* cpu
// ){
//     // nothing happens
// }
//

// =========================
// OPCODES
// =========================

static void init_opcodes(CPU6502 *c) {
  memset(c->op, 0, sizeof(c->op));

  c->op[0xA9] = (Opcode){LDA, IMM};
  c->op[0xA5] = (Opcode){LDA, ZP};
  c->op[0xAD] = (Opcode){LDA, ABS};

  c->op[0xA2] = (Opcode){LDX, IMM};
  c->op[0xA6] = (Opcode){LDX, ZP};
  c->op[0xAE] = (Opcode){LDX, ABS};

  c->op[0xA0] = (Opcode){LDY, IMM};

  c->op[0x69] = (Opcode){ADC, IMM};

  c->op[0xF0] = (Opcode){BEQ, REL};
  c->op[0xD0] = (Opcode){BNE, REL};

  c->op[0x4C] = (Opcode){JMP, ABS};

  c->op[0xEA] = (Opcode){NOP, IMP};
  c->op[0x00] = (Opcode){BRK, IMP};
}

// =========================
// STEP
// =========================

// static void step(CPU6502 *c) {
//   uint8_t op = fetch(c);
//   Opcode ins = c->op[op];
//
//   if (!ins.fn) {
//     printf("Unknown opcode %02X\n", op);
//     c->running = false;
//     return;
//   }
//
//   ins.fn(c, ins.mode);
// }


static const uint8_t opcode_cycles[256] = {
    [0xA9] = 2, // LDA imm
    [0xA5] = 3, // LDA zp
    [0xAD] = 4, // LDA abs

    [0x69] = 2, // ADC imm

    [0x4C] = 3, // JMP abs

    [0xEA] = 2, // NOP
    [0x00] = 7  // BRK
};

// static void step(CPU6502* c)
// {
//     uint8_t opcode = fetch(c);
//     Opcode ins = c->op[opcode];
//
//     if (!ins.fn) {
//         printf("Unknown opcode %02X\n", opcode);
//         c->running = false;
//         return;
//     }
//
//     ins.fn(c, ins.mode);
//
//     c->cycles += opcode_cycles[opcode];
// }


void run_cycles(CPU6502* c, int cycles)
{
    c->cycles_left += cycles;

    while (c->cycles_left > 0 && c->running) {

        uint8_t opcode = fetch(c);
        Opcode ins = c->op[opcode];

        if (!ins.fn) {
            c->running = false;
            return;
        }

        uint8_t cost = opcode_cycles[opcode];

        ins.fn(c, ins.mode);

        c->cycles_left -= cost;
        c->cycles += cost;
    }
}

int main(void) {
  Memory mem = {0};

  // простая тестовая программа:
  // LDA #$01
  // ADC #$01
  // BRK
  uint8_t program[] = {
      0xA9, 0x01, // LDA #1
      0x69, 0x01, // ADC #1
      0x00        // BRK
  };

  // загрузка в память
  for (size_t i = 0; i < sizeof(program); i++) {
    mem.data[0x8000 + i] = program[i];
  }

  // reset vector
  mem.data[0xFFFC] = 0x00;
  mem.data[0xFFFD] = 0x80;

  CPU6502 cpu = {0};
  cpu.mem = &mem;
  cpu.sp = 0xFD;

  init_opcodes(&cpu);

  // reset
  cpu.pc = mem_read(cpu.mem, 0xFFFC) | (mem_read(cpu.mem, 0xFFFD) << 8);

  cpu.running = true;

  // run
  // while (cpu.running) {
  //   step(&cpu);
  // }
  run_cycles(&cpu, 100000);

  printf("A = %u\n", cpu.a);
  printf("X = %u\n", cpu.x);
  printf("Y = %u\n", cpu.y);
  printf("Cycles = %llu\n", cpu.cycles);

  return 0;
}
