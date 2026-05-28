/* mos6502.c: a library implementing a fully functional MOS 6502 CPU in C */

#include "mos6502.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


// =========================
// MEMORY ACCESS
// =========================

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


static Operand resolve_operand(CPU6502 *c, AddrMode mode)
{
    Operand op = {0};

    switch (mode)
    {
        case IMM:
            op.addr = c->pc++;
            op.value = mem_read(c->mem, op.addr);
            break;

        case ZP:
        case ABS:
        case ZPX:
        case ZPY:
        case ABX:
        case ABY:
        case IND:
        case IZX:
        case IZY:
        case REL:
            op.addr = get_addr(c, mode);
            op.value = mem_read(c->mem, op.addr);
            break;

        case IMP:
        case ACC:
        default:
            op.addr = 0;
            op.value = c->a;
            break;
    }

    return op;
}
// =========================
// LOAD / STORE
// =========================

// Load accumulator
static void LDA(CPU6502 *c, Operand op)
{
    c->a = op.value;
    update_zn(c, c->a);
}

// Load X register
static void LDX(CPU6502 *c, Operand op)
{
    c->x = op.value;
    update_zn(c, c->x);
}

// Load Y register
static void LDY(CPU6502 *c, Operand op)
{
    c->y = op.value;
    update_zn(c, c->y);
}

// Store accumulator
static void STA(CPU6502 *c, Operand op)
{
    mem_write(c->mem, op.addr, c->a);
}


// Store X register
static void STX(CPU6502 *c, Operand op)
{
    mem_write(c->mem, op.addr, c->x);
}

// Store Y register
static void STY(CPU6502 *c, Operand op)
{
    mem_write(c->mem, op.addr, c->y);
}

// =========================
// TRANSFER
// =========================

// A -> X
static void TAX(CPU6502 *c, Operand op)
{
    (void)op;

    c->x = c->a;
    update_zn(c, c->x);
}

// A -> Y
static void TAY(CPU6502 *c, Operand op)
{
    (void)op;

    c->y = c->a;
    update_zn(c, c->y);
}

// SP -> X
static void TSX(CPU6502 *c, Operand op)
{
    (void)op;

    c->x = c->sp;
    update_zn(c, c->x);
}

// X -> A
static void TXA(CPU6502 *c, Operand op)
{
    (void)op;

    c->a = c->x;
    update_zn(c, c->a);
}

// X -> SP
static void TXS(CPU6502 *c, Operand op)
{
    (void)op;

    c->sp = c->x;
}

// Y -> A
static void TYA(CPU6502 *c, Operand op)
{
    (void)op;

    c->a = c->y;
    update_zn(c, c->a);
}
// =========================
// STACK
// =========================

// Push accumulator
static void PHA(CPU6502 *c, Operand op)
{
    (void)op;

    push(c, c->a);
}

// Push processor status
static void PHP(CPU6502 *c, Operand op)
{
    (void)op;

    push(
        c,
        c->status |
        (1 << FLAG_B) |
        (1 << FLAG_U)
    );
}

// Pull accumulator
static void PLA(CPU6502 *c, Operand op)
{
    (void)op;

    c->a = pop(c);

    update_zn(c, c->a);
}

// Pull processor status
static void PLP(CPU6502 *c, Operand op)
{
    (void)op;

    c->status = pop(c);

    // unused flag is always set
    c->status |= (1 << FLAG_U);
}

// =========================
// INCREMENTS / DECREMENTS
// =========================

// Decrement memory
static void DEC(CPU6502 *c, Operand op)
{
    uint8_t value = op.value - 1;

    mem_write(c->mem, op.addr, value);

    update_zn(c, value);
}

// Decrement X
static void DEX(CPU6502 *c, Operand op)
{
    (void)op;

    c->x--;

    update_zn(c, c->x);
}

// Decrement Y
static void DEY(CPU6502 *c, Operand op)
{
    (void)op;

    c->y--;

    update_zn(c, c->y);
}

// Increment memory
static void INC(CPU6502 *c, Operand op)
{
    uint8_t value = op.value + 1;

    mem_write(c->mem, op.addr, value);

    update_zn(c, value);
}

// Increment X
static void INX(CPU6502 *c, Operand op)
{
    (void)op;

    c->x++;

    update_zn(c, c->x);
}

// Increment Y
static void INY(CPU6502 *c, Operand op)
{
    (void)op;

    c->y++;

    update_zn(c, c->y);
}

// =========================
// ARITHMETIC
// =========================

static bool get_flag(CPU6502 *c, Flag f)
{
    return (c->status & (1 << f)) != 0;
}

// Add with carry
static void ADC(CPU6502 *c, Operand op)
{
    uint16_t result =
        c->a +
        op.value +
        get_flag(c, FLAG_C);

    // Carry flag
    set_flag(c, FLAG_C, result > 0xFF);

    // Overflow flag
    set_flag(
        c,
        FLAG_V,
        (~(c->a ^ op.value) &
         (c->a ^ result) &
         0x80)
    );

    // Store result
    c->a = result & 0xFF;

    // Update Z/N flags
    update_zn(c, c->a);
}

// Subtract with carry
static void SBC(CPU6502 *c, Operand op)
{
    uint8_t value = op.value ^ 0xFF;

    uint16_t result =
        c->a +
        value +
        get_flag(c, FLAG_C);

    // Carry flag
    set_flag(c, FLAG_C, result > 0xFF);

    // Overflow flag
    set_flag(
        c,
        FLAG_V,
        (~(c->a ^ value) &
         (c->a ^ result) &
         0x80)
    );

    // Store result
    c->a = result & 0xFF;

    // Update Z/N flags
    update_zn(c, c->a);
}

// =========================
// LOGICAL
// =========================

// AND
// A = A & M
static void AND(CPU6502 *c, Operand op)
{
    c->a &= op.value;

    update_zn(c, c->a);
}

// EOR (XOR)
// A = A ^ M
static void EOR(CPU6502 *c, Operand op)
{
    c->a ^= op.value;

    update_zn(c, c->a);
}

// ORA (OR)
// A = A | M
static void ORA(CPU6502 *c, Operand op)
{
    c->a |= op.value;

    update_zn(c, c->a);
}

// =========================
// SHIFT / ROTATE
// =========================

// Arithmetic shift left
static void ASL(CPU6502 *c, Operand op)
{
    uint8_t value = op.value;

    set_flag(c, FLAG_C, value & 0x80);

    value <<= 1;

    mem_write(c->mem, op.addr, value);

    update_zn(c, value);
}

// Arithmetic shift left (accumulator)
static void ASL_A(CPU6502 *c, Operand op)
{
    (void)op;

    set_flag(c, FLAG_C, c->a & 0x80);

    c->a <<= 1;

    update_zn(c, c->a);
}

// Logical shift right
static void LSR(CPU6502 *c, Operand op)
{
    uint8_t value = op.value;

    set_flag(c, FLAG_C, value & 0x01);

    value >>= 1;

    mem_write(c->mem, op.addr, value);

    update_zn(c, value);
}

// Logical shift right (accumulator)
static void LSR_A(CPU6502 *c, Operand op)
{
    (void)op;

    set_flag(c, FLAG_C, c->a & 0x01);

    c->a >>= 1;

    update_zn(c, c->a);
}

// Rotate left
static void ROL(CPU6502 *c, Operand op)
{
    uint8_t value = op.value;

    uint8_t old_carry = get_flag(c, FLAG_C);

    set_flag(c, FLAG_C, value & 0x80);

    value = (value << 1) | old_carry;

    mem_write(c->mem, op.addr, value);

    update_zn(c, value);
}

// Rotate left (accumulator)
static void ROL_A(CPU6502 *c, Operand op)
{
    (void)op;

    uint8_t old_carry = get_flag(c, FLAG_C);

    set_flag(c, FLAG_C, c->a & 0x80);

    c->a = (c->a << 1) | old_carry;

    update_zn(c, c->a);
}

// Rotate right
static void ROR(CPU6502 *c, Operand op)
{
    uint8_t value = op.value;

    uint8_t old_carry = get_flag(c, FLAG_C);

    set_flag(c, FLAG_C, value & 0x01);

    value = (value >> 1) | (old_carry << 7);

    mem_write(c->mem, op.addr, value);

    update_zn(c, value);
}

// Rotate right (accumulator)
static void ROR_A(CPU6502 *c, Operand op)
{
    (void)op;

    uint8_t old_carry = get_flag(c, FLAG_C);

    set_flag(c, FLAG_C, c->a & 0x01);

    c->a = (c->a >> 1) | (old_carry << 7);

    update_zn(c, c->a);
}

// =========================
// FLAGS
// =========================

// Clear carry flag
static void CLC(CPU6502 *c, Operand op)
{
    (void)op;

    set_flag(c, FLAG_C, false);
}

// Clear decimal flag
static void CLD(CPU6502 *c, Operand op)
{
    (void)op;

    set_flag(c, FLAG_D, false);
}

// Clear interrupt disable flag
static void CLI(CPU6502 *c, Operand op)
{
    (void)op;

    set_flag(c, FLAG_I, false);
}

// Clear overflow flag
static void CLV(CPU6502 *c, Operand op)
{
    (void)op;

    set_flag(c, FLAG_V, false);
}

// Set carry flag
static void SEC(CPU6502 *c, Operand op)
{
    (void)op;

    set_flag(c, FLAG_C, true);
}

// Set decimal flag
static void SED(CPU6502 *c, Operand op)
{
    (void)op;

    set_flag(c, FLAG_D, true);
}

// Set interrupt disable flag
static void SEI(CPU6502 *c, Operand op)
{
    (void)op;

    set_flag(c, FLAG_I, true);
}

// =========================
// COMPARISON
// =========================

static void update_cmp_flags(
    CPU6502 *c,
    uint8_t reg,
    uint8_t value
)
{
    uint16_t result = (uint16_t)reg - value;

    set_flag(c, FLAG_C, reg >= value);
    set_flag(c, FLAG_Z, reg == value);
    set_flag(c, FLAG_N, result & 0x80);
}

// Compare accumulator
static void CMP(CPU6502 *c, Operand op)
{
    update_cmp_flags(
        c,
        c->a,
        op.value
    );
}

// Compare X register
static void CPX(CPU6502 *c, Operand op)
{
    update_cmp_flags(
        c,
        c->x,
        op.value
    );
}

// Compare Y register
static void CPY(CPU6502 *c, Operand op)
{
    update_cmp_flags(
        c,
        c->y,
        op.value
    );
}
// =========================
// BIT
// =========================

static void BIT(CPU6502 *c, Operand op)
{
    uint8_t result = c->a & op.value;

    // Zero flag
    set_flag(c, FLAG_Z, result == 0);

    // Negative flag = bit 7
    set_flag(c, FLAG_N, op.value & 0x80);

    // Overflow flag = bit 6
    set_flag(c, FLAG_V, op.value & 0x40);
}

// =========================
// BRANCH HELPER
// =========================

static void branch(CPU6502 *c, bool condition, Operand op)
{
    if (condition) {
        c->pc = op.addr;
    }
}

// =========================
// INTERRUPS
// =========================

static void IRQ(CPU6502 *c)
{
    if (c->status & (1 << FLAG_I))
        return;

    push(c, (c->pc >> 8) & 0xFF);
    push(c, c->pc & 0xFF);

    push(c, c->status);

    set_flag(c, FLAG_I, true);

    uint16_t lo = mem_read(c->mem, 0xFFFE);
    uint16_t hi = mem_read(c->mem, 0xFFFF);

    c->pc = (hi << 8) | lo;
}

static void NMI(CPU6502 *c)
{
    push(c, (c->pc >> 8) & 0xFF);
    push(c, c->pc & 0xFF);

    push(c, c->status);

    set_flag(c, FLAG_I, true);

    uint16_t lo = mem_read(c->mem, 0xFFFA);
    uint16_t hi = mem_read(c->mem, 0xFFFB);

    c->pc = (hi << 8) | lo;
}

// =========================
// CONDITIONAL BRANCHES
// =========================

static void BCC(CPU6502 *c, Operand op)
{
    branch(c, !(c->status & (1 << FLAG_C)), op);
}

static void BCS(CPU6502 *c, Operand op)
{
    branch(c, (c->status & (1 << FLAG_C)), op);
}

static void BEQ(CPU6502 *c, Operand op)
{
    branch(c, (c->status & (1 << FLAG_Z)), op);
}

static void BMI(CPU6502 *c, Operand op)
{
    branch(c, (c->status & (1 << FLAG_N)), op);
}

static void BNE(CPU6502 *c, Operand op)
{
    branch(c, !(c->status & (1 << FLAG_Z)), op);
}

static void BPL(CPU6502 *c, Operand op)
{
    branch(c, !(c->status & (1 << FLAG_N)), op);
}

static void BVC(CPU6502 *c, Operand op)
{
    branch(c, !(c->status & (1 << FLAG_V)), op);
}

static void BVS(CPU6502 *c, Operand op)
{
    branch(c, (c->status & (1 << FLAG_V)), op);
}

// =========================
// JUMPS & SUBROUTINES
// =========================

// JMP (absolute or indirect handled via operand.addr in dispatcher)
static void JMP(CPU6502 *c, Operand op)
{
    c->pc = op.addr;
}

// JSR (Jump to Subroutine)
static void JSR(CPU6502 *c, Operand op)
{
    uint16_t return_addr = c->pc - 1;

    push(c, (return_addr >> 8) & 0xFF);
    push(c, return_addr & 0xFF);

    c->pc = op.addr;
}

// RTS (Return from Subroutine)
static void RTS(CPU6502 *c, Operand op)
{
    (void)op;

    uint8_t lo = pop(c);
    uint8_t hi = pop(c);

    c->pc = ((hi << 8) | lo) + 1;
}

// =========================
// INTERRUPTS
// =========================

static void BRK(CPU6502 *c, Operand op)
{
    (void)op;

    c->pc++;

    // push PC high
    push(c, (c->pc >> 8) & 0xFF);

    // push PC low
    push(c, c->pc & 0xFF);

    // push status with B + U set
    push(c, c->status | (1 << FLAG_B) | (1 << FLAG_U));

    // disable interrupts
    set_flag(c, FLAG_I, true);

    // load interrupt vector
    uint16_t lo = mem_read(c->mem, 0xFFFE);
    uint16_t hi = mem_read(c->mem, 0xFFFF);

    c->pc = (hi << 8) | lo;
}

// =========================
// OTHER
// =========================

static void NOP(CPU6502 *c, Operand op)
{
    (void)c;
    (void)op;
}

// =========================
// OPCODES
// =========================

static void init_opcodes(CPU6502 *c)
{
    memset(c->op, 0, sizeof(c->op));

    // =========================
    // LDA
    // =========================
    c->op[0xA9] = (Opcode){LDA, IMM, 2};
    c->op[0xA5] = (Opcode){LDA, ZP, 3};
    c->op[0xB5] = (Opcode){LDA, ZPX, 4};
    c->op[0xAD] = (Opcode){LDA, ABS, 4};
    c->op[0xBD] = (Opcode){LDA, ABX, 4};
    c->op[0xB9] = (Opcode){LDA, ABY, 4};
    c->op[0xA1] = (Opcode){LDA, IZX, 6};
    c->op[0xB1] = (Opcode){LDA, IZY, 5};

    // =========================
    // LDX
    // =========================
    c->op[0xA2] = (Opcode){LDX, IMM, 2};
    c->op[0xA6] = (Opcode){LDX, ZP, 3};
    c->op[0xB6] = (Opcode){LDX, ZPY, 4};
    c->op[0xAE] = (Opcode){LDX, ABS, 4};
    c->op[0xBE] = (Opcode){LDX, ABY, 4};

    // =========================
    // LDY
    // =========================
    c->op[0xA0] = (Opcode){LDY, IMM, 2};
    c->op[0xA4] = (Opcode){LDY, ZP, 3};
    c->op[0xB4] = (Opcode){LDY, ZPX, 4};
    c->op[0xAC] = (Opcode){LDY, ABS, 4};
    c->op[0xBC] = (Opcode){LDY, ABX, 4};

    // =========================
    // STA
    // =========================
    c->op[0x85] = (Opcode){STA, ZP, 3};
    c->op[0x95] = (Opcode){STA, ZPX, 4};
    c->op[0x8D] = (Opcode){STA, ABS, 4};
    c->op[0x9D] = (Opcode){STA, ABX, 5};
    c->op[0x99] = (Opcode){STA, ABY, 5};
    c->op[0x81] = (Opcode){STA, IZX, 6};
    c->op[0x91] = (Opcode){STA, IZY, 6};

    // =========================
    // STX
    // =========================
    c->op[0x86] = (Opcode){STX, ZP, 3};
    c->op[0x96] = (Opcode){STX, ZPY, 4};
    c->op[0x8E] = (Opcode){STX, ABS, 4};

    // =========================
    // STY
    // =========================
    c->op[0x84] = (Opcode){STY, ZP, 3};
    c->op[0x94] = (Opcode){STY, ZPX, 4};
    c->op[0x8C] = (Opcode){STY, ABS, 4};

    // =========================
    // TAX / TAY / TXA / TYA
    // =========================
    c->op[0xAA] = (Opcode){TAX, IMP, 2};
    c->op[0xA8] = (Opcode){TAY, IMP, 2};
    c->op[0x8A] = (Opcode){TXA, IMP, 2};
    c->op[0x98] = (Opcode){TYA, IMP, 2};

    // =========================
    // TSX / TXS
    // =========================
    c->op[0xBA] = (Opcode){TSX, IMP, 2};
    c->op[0x9A] = (Opcode){TXS, IMP, 2};

    // =========================
    // PHA / PHP / PLA / PLP
    // =========================
    c->op[0x48] = (Opcode){PHA, IMP, 3};
    c->op[0x08] = (Opcode){PHP, IMP, 3};
    c->op[0x68] = (Opcode){PLA, IMP, 4};
    c->op[0x28] = (Opcode){PLP, IMP, 4};

    // =========================
    // INC / DEC (memory)
    // =========================
    c->op[0xE6] = (Opcode){INC, ZP, 5};
    c->op[0xF6] = (Opcode){INC, ZPX, 6};
    c->op[0xEE] = (Opcode){INC, ABS, 6};
    c->op[0xFE] = (Opcode){INC, ABX, 7};

    c->op[0xC6] = (Opcode){DEC, ZP, 5};
    c->op[0xD6] = (Opcode){DEC, ZPX, 6};
    c->op[0xCE] = (Opcode){DEC, ABS, 6};
    c->op[0xDE] = (Opcode){DEC, ABX, 7};

    // =========================
    // INX / INY / DEX / DEY
    // =========================
    c->op[0xE8] = (Opcode){INX, IMP, 2};
    c->op[0xC8] = (Opcode){INY, IMP, 2};
    c->op[0xCA] = (Opcode){DEX, IMP, 2};
    c->op[0x88] = (Opcode){DEY, IMP, 2};

    // =========================
    // ADC
    // =========================
    c->op[0x69] = (Opcode){ADC, IMM, 2};
    c->op[0x65] = (Opcode){ADC, ZP, 3};
    c->op[0x75] = (Opcode){ADC, ZPX, 4};
    c->op[0x6D] = (Opcode){ADC, ABS, 4};
    c->op[0x7D] = (Opcode){ADC, ABX, 4};
    c->op[0x79] = (Opcode){ADC, ABY, 4};
    c->op[0x61] = (Opcode){ADC, IZX, 6};
    c->op[0x71] = (Opcode){ADC, IZY, 5};

    // =========================
    // SBC
    // =========================
    c->op[0xE9] = (Opcode){SBC, IMM, 2};
    c->op[0xE5] = (Opcode){SBC, ZP, 3};
    c->op[0xF5] = (Opcode){SBC, ZPX, 4};
    c->op[0xED] = (Opcode){SBC, ABS, 4};
    c->op[0xFD] = (Opcode){SBC, ABX, 4};
    c->op[0xF9] = (Opcode){SBC, ABY, 4};
    c->op[0xE1] = (Opcode){SBC, IZX, 6};
    c->op[0xF1] = (Opcode){SBC, IZY, 5};

    // =========================
    // LOGIC
    // =========================
    c->op[0x29] = (Opcode){AND, IMM, 2};
    c->op[0x25] = (Opcode){AND, ZP, 3};
    c->op[0x35] = (Opcode){AND, ZPX, 4};
    c->op[0x2D] = (Opcode){AND, ABS, 4};
    c->op[0x3D] = (Opcode){AND, ABX, 4};
    c->op[0x39] = (Opcode){AND, ABY, 4};
    c->op[0x21] = (Opcode){AND, IZX, 6};
    c->op[0x31] = (Opcode){AND, IZY, 5};

    c->op[0x09] = (Opcode){ORA, IMM, 2};
    c->op[0x05] = (Opcode){ORA, ZP, 3};
    c->op[0x15] = (Opcode){ORA, ZPX, 4};
    c->op[0x0D] = (Opcode){ORA, ABS, 4};
    c->op[0x1D] = (Opcode){ORA, ABX, 4};
    c->op[0x19] = (Opcode){ORA, ABY, 4};
    c->op[0x01] = (Opcode){ORA, IZX, 6};
    c->op[0x11] = (Opcode){ORA, IZY, 5};

    c->op[0x49] = (Opcode){EOR, IMM, 2};
    c->op[0x45] = (Opcode){EOR, ZP, 3};
    c->op[0x55] = (Opcode){EOR, ZPX, 4};
    c->op[0x4D] = (Opcode){EOR, ABS, 4};
    c->op[0x5D] = (Opcode){EOR, ABX, 4};
    c->op[0x59] = (Opcode){EOR, ABY, 4};
    c->op[0x41] = (Opcode){EOR, IZX, 6};
    c->op[0x51] = (Opcode){EOR, IZY, 5};

    c->op[0x24] = (Opcode){BIT, ZP, 3};
    c->op[0x2C] = (Opcode){BIT, ABS, 4};

    // =========================
    // SHIFT / ROTATE
    // =========================
    c->op[0x0A] = (Opcode){ASL_A, ACC, 2};
    c->op[0x06] = (Opcode){ASL, ZP, 5};
    c->op[0x16] = (Opcode){ASL, ZPX, 6};
    c->op[0x0E] = (Opcode){ASL, ABS, 6};
    c->op[0x1E] = (Opcode){ASL, ABX, 7};

    c->op[0x4A] = (Opcode){LSR_A, ACC, 2};
    c->op[0x46] = (Opcode){LSR, ZP, 5};
    c->op[0x56] = (Opcode){LSR, ZPX, 6};
    c->op[0x4E] = (Opcode){LSR, ABS, 6};
    c->op[0x5E] = (Opcode){LSR, ABX, 7};

    c->op[0x2A] = (Opcode){ROL_A, ACC, 2};
    c->op[0x26] = (Opcode){ROL, ZP, 5};
    c->op[0x36] = (Opcode){ROL, ZPX, 6};
    c->op[0x2E] = (Opcode){ROL, ABS, 6};
    c->op[0x3E] = (Opcode){ROL, ABX, 7};

    c->op[0x6A] = (Opcode){ROR_A, ACC, 2};
    c->op[0x66] = (Opcode){ROR, ZP, 5};
    c->op[0x76] = (Opcode){ROR, ZPX, 6};
    c->op[0x6E] = (Opcode){ROR, ABS, 6};
    c->op[0x7E] = (Opcode){ROR, ABX, 7};

    // =========================
    // FLAGS
    // =========================
    c->op[0x18] = (Opcode){CLC, IMP, 2};
    c->op[0x38] = (Opcode){SEC, IMP, 2};
    c->op[0x58] = (Opcode){CLI, IMP, 2};
    c->op[0x78] = (Opcode){SEI, IMP, 2};
    c->op[0xB8] = (Opcode){CLV, IMP, 2};
    c->op[0xD8] = (Opcode){CLD, IMP, 2};
    c->op[0xF8] = (Opcode){SED, IMP, 2};

    // =========================
    // CMP / CPX / CPY
    // =========================
    c->op[0xC9] = (Opcode){CMP, IMM, 2};
    c->op[0xC5] = (Opcode){CMP, ZP, 3};
    c->op[0xD5] = (Opcode){CMP, ZPX, 4};
    c->op[0xCD] = (Opcode){CMP, ABS, 4};
    c->op[0xDD] = (Opcode){CMP, ABX, 4};
    c->op[0xD9] = (Opcode){CMP, ABY, 4};
    c->op[0xC1] = (Opcode){CMP, IZX, 6};
    c->op[0xD1] = (Opcode){CMP, IZY, 5};

    c->op[0xE0] = (Opcode){CPX, IMM, 2};
    c->op[0xE4] = (Opcode){CPX, ZP, 3};
    c->op[0xEC] = (Opcode){CPX, ABS, 4};

    c->op[0xC0] = (Opcode){CPY, IMM, 2};
    c->op[0xC4] = (Opcode){CPY, ZP, 3};
    c->op[0xCC] = (Opcode){CPY, ABS, 4};

    // =========================
    // BRANCHES
    // =========================
    c->op[0x90] = (Opcode){BCC, REL, 2};
    c->op[0xB0] = (Opcode){BCS, REL, 2};
    c->op[0xF0] = (Opcode){BEQ, REL, 2};
    c->op[0x30] = (Opcode){BMI, REL, 2};
    c->op[0xD0] = (Opcode){BNE, REL, 2};
    c->op[0x10] = (Opcode){BPL, REL, 2};
    c->op[0x50] = (Opcode){BVC, REL, 2};
    c->op[0x70] = (Opcode){BVS, REL, 2};

    // =========================
    // JUMPS
    // =========================
    c->op[0x4C] = (Opcode){JMP, ABS, 3};
    c->op[0x6C] = (Opcode){JMP, IND, 5};
    c->op[0x20] = (Opcode){JSR, ABS, 6};
    c->op[0x60] = (Opcode){RTS, IMP, 6};

    // =========================
    // SYSTEM
    // =========================
    c->op[0xEA] = (Opcode){NOP, IMP, 2};
    c->op[0x00] = (Opcode){BRK, IMP, 7};
}


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

        // 1. addressing mode
        Operand op = resolve_operand(c, ins.mode);

        // 2. base cycles
        int used = ins.cycles;

        // 3. execute instruction
        ins.fn(c, op);

        // 4. add addressing penalty
        /* used += op.extra_cycles; */

        c->cycles_left -= used;
        c->cycles += used;
    }
}

/* int main(int argc, char **argv) */
/* { */
/*     if (argc < 2) { */
/*         printf("Usage: %s <binary file>\n", argv[0]); */
/*         return 1; */
/*     } */

/*     FILE *f = fopen(argv[1], "rb"); */
/*     if (!f) { */
/*         perror("fopen"); */
/*         return 1; */
/*     } */

/*     fseek(f, 0, SEEK_END); */
/*     size_t size = ftell(f); */
/*     fseek(f, 0, SEEK_SET); */

/*     uint8_t *program = malloc(size); */
/*     if (!program) { */
/*         perror("malloc"); */
/*         return 1; */
/*     } */

/*     fread(program, 1, size, f); */
/*     fclose(f); */

/*     Memory mem = {0}; */

/*     // загрузка в память NES-style */
/*     for (size_t i = 0; i < size; i++) { */
/*         mem.data[0x8000 + i] = program[i]; */
/*     } */

/*     free(program); */

/*     // reset vector */
/*     mem.data[0xFFFC] = 0x00; */
/*     mem.data[0xFFFD] = 0x80; */

/*     CPU6502 cpu = {0}; */
/*     cpu.mem = &mem; */
/*     cpu.sp = 0xFD; */

/*     init_opcodes(&cpu); */

/*     cpu.pc = */
/*         mem_read(cpu.mem, 0xFFFC) | */
/*         (mem_read(cpu.mem, 0xFFFD) << 8); */

/*     cpu.running = true; */

/*     run_cycles(&cpu, 100000); */

/*     printf("A = %u\n", cpu.a); */
/*     printf("X = %u\n", cpu.x); */
/*     printf("Y = %u\n", cpu.y); */
/*     printf("Cycles = %llu\n", cpu.cycles); */

/*     return 0; */
/* } */


void cpu_init(CPU6502 *c, Memory *m, GPU *g)
{
    memset(c, 0, sizeof(*c));

    c->mem = m;
    c->gpu = g;

    c->sp = 0xFD;
    c->running = true;

    init_opcodes(c);
}

void cpu_reset(CPU6502 *c)
{
    uint16_t lo = mem_read(c->mem, 0xFFFC);
    uint16_t hi = mem_read(c->mem, 0xFFFD);

    c->pc = (hi << 8) | lo;
    c->running = true;
}

void cpu_run(CPU6502 *c, int cycles)
{
    c->cycles_left += cycles;

    while (c->running && c->cycles_left > 0)
    {
        uint8_t opcode = fetch(c);
        Opcode ins = c->op[opcode];

        if (!ins.fn)
        {
            printf("Unknown opcode: %02X\n", opcode);
            c->running = false;
            break;
        }

        Operand op = resolve_operand(c, ins.mode);

        ins.fn(c, op);

        c->cycles_left -= ins.cycles;
        c->cycles += ins.cycles;
    }
}
