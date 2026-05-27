/* cpu.c: a library implementing a fully functional MOS 6502 CPU in C */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define MEM_SIZE 0x10000

typedef struct {
    uint8_t data[MEM_SIZE];
} Memory;

typedef enum {
    FLAG_C = 0,
    FLAG_Z,
    FLAG_I,
    FLAG_D,
    FLAG_B,
    FLAG_U,
    FLAG_V,
    FLAG_N
} Status;




typedef enum {
    IMM,
    ZP,
    ABS,
    IND,
    ZPX,
    ZPY,
    ABX,
    ABY,
    IMP
} AddrMode;


/* typedef enum { */
/*     IMM, */
/*     ZP, */
/*     ABS, */
/*     IND, */
/*     ZPX, */
/*     ZPY, */
/*     ABX, */
/*     ABY, */
/*     IMP, */

/*     // 🔥 MISSING (ОЧЕНЬ ВАЖНО) */
/*     IZX,   // ($zp,X) */
/*     IZY,   // ($zp),Y */
/*     ACC, */
/*     REL */
/* } AddrMode; */


typedef struct CPU6502 CPU6502;

typedef void (*InstrFn)(CPU6502* cpu, AddrMode mode);

typedef struct {
    InstrFn fn;
    AddrMode mode;
} Opcode;

typedef struct CPU6502 CPU6502;

typedef void (*OpcodeFn)(CPU6502*);

/* struct CPU6502 { */

/*     Memory* mem; */

/*     uint8_t a; */
/*     uint8_t x; */
/*     uint8_t y; */

/*     uint16_t pc; */

/*     uint8_t sp; */

/*     uint8_t status; */

/*     bool running; */

/*     uint64_t cycles; */

/*     OpcodeFn op[256]; */
/* }; */


struct CPU6502 {
    uint8_t a, x, y;
    uint8_t sp;
    uint8_t status;
    uint16_t pc;

    Memory* mem;

    Opcode op[256];
};

/*=========================
MEMORY
=========================*/

void mem_check(uint16_t addr) {

    if (addr >= MEM_SIZE) {

        fprintf(
            stderr,
            "Memory access out of bounds\n"
        );

        exit(1);
    }
}

uint8_t mem_read(
    Memory* mem,
    uint16_t addr
) {

    mem_check(addr);

    return mem->data[addr];
}

void mem_write(
    Memory* mem,
    uint16_t addr,
    uint8_t value
) {

    mem_check(addr);

    if (addr == 0xD001) {

        putchar(value);

        fflush(stdout);
    }

    mem->data[addr] =
        value
        &
        0xFF;
}

void mem_load(
    Memory* mem,
    uint16_t addr,
    const uint8_t* blob,
    size_t size
) {

    for (
        size_t i = 0;
        i < size;
        i++
    ) {

        mem_write(
            mem,
            addr + i,
            blob[i]
        );
    }
}

/*=========================
CPU helpers
=========================*/

void set_flag(
    CPU6502* cpu,
    int f,
    bool v
) {

    if (v)
        cpu->status |=
            (1 << f);

    else
        cpu->status &=
            ~(1 << f);
}

uint8_t cpu_read(
    CPU6502* cpu,
    uint16_t addr
) {

    cpu->cycles++;

    return mem_read(
        cpu->mem,
        addr
    );
}

void cpu_write(
    CPU6502* cpu,
    uint16_t addr,
    uint8_t value
) {

    cpu->cycles++;

    mem_write(
        cpu->mem,
        addr,
        value
    );
}

uint16_t read_word(
    CPU6502* cpu,
    uint16_t addr
) {

    uint8_t lo =
        cpu_read(
            cpu,
            addr
        );

    uint8_t hi =
        cpu_read(
            cpu,
            (
                addr
                +
                1
            )
            &
            0xFFFF
        );

    return
        lo
        |
        (
            hi
            <<
            8
        );
}

void push(
    CPU6502* cpu,
    uint8_t v
) {

    cpu_write(
        cpu,
        0x100
        +
        cpu->sp,
        v
    );

    cpu->sp--;
}

uint8_t pop(
    CPU6502* cpu
) {

    cpu->sp++;

    return cpu_read(
        cpu,
        0x100
        +
        cpu->sp
    );
}

uint8_t fetch(
    CPU6502* cpu
) {

    uint8_t v =
        cpu_read(
            cpu,
            cpu->pc
        );

    cpu->pc =
        (
            cpu->pc
            +
            1
        )
        &
        0xFFFF;

    return v;
}

uint16_t fetch_word(
    CPU6502* cpu
) {

    return
        fetch(cpu)
        |
        (
            fetch(cpu)
            <<
            8
        );
}

/*=========================
ADDRESSING
=========================*/

uint8_t imm(
    CPU6502* cpu
) {

    return fetch(cpu);
}

uint16_t abs_addr(
    CPU6502* cpu
) {

    return fetch_word(
        cpu
    );
}

/*
  there  i will describe all instructions
*/

/* void lda_imm( */
/*     CPU6502* cpu */
/* ) { */

/*     cpu->a = */
/*         imm(cpu); */

/*     set_flag( */
/*         cpu, */
/*         FLAG_Z, */
/*         cpu->a == 0 */
/*     ); */

/*     set_flag( */
/*         cpu, */
/*         FLAG_N, */
/*         cpu->a & 0x80 */
/*     ); */
/* } */

/* void sta_abs( */
/*     CPU6502* cpu */
/* ) { */

/*     cpu_write( */
/*         cpu, */
/*         abs_addr(cpu), */
/*         cpu->a */
/*     ); */
/* } */

/* void jmp_abs( */
/*     CPU6502* cpu */
/* ) { */

/*     cpu->pc = */
/*         abs_addr(cpu); */
/* } */

/* void jsr_abs( */
/*     CPU6502* cpu */
/* ) { */

/*     uint16_t addr = */
/*         abs_addr(cpu); */

/*     uint16_t ret = */
/*         ( */
/*             cpu->pc */
/*             - */
/*             1 */
/*         ) */
/*         & */
/*         0xFFFF; */

/*     push( */
/*         cpu, */
/*         ( */
/*             ret */
/*             >> */
/*             8 */
/*         ) */
/*     ); */

/*     push( */
/*         cpu, */
/*         ret */
/*     ); */

/*     cpu->pc = */
/*         addr; */
/* } */

/* void rts_imp( */
/*     CPU6502* cpu */
/* ) { */

/*     uint8_t lo = */
/*         pop(cpu); */

/*     uint8_t hi = */
/*         pop(cpu); */

/*     cpu->pc = */
/*         ( */
/*             ( */
/*                 hi */
/*                 << */
/*                 8 */
/*             ) */
/*             | */
/*             lo */
/*         ) */
/*         + */
/*         1; */
/* } */

/* void brk_imp( */
/*     CPU6502* cpu */
/* ) { */

/*     cpu->running = */
/*         false; */

/*     cpu->pc++; */
/* } */

/* void clc_imp( */
/*     CPU6502* cpu */
/* ) { */

/*     set_flag( */
/*         cpu, */
/*         FLAG_C, */
/*         false */
/*     ); */
/* } */

/* void adc_imm( */
/*     CPU6502* cpu */
/* ) { */

/*     uint8_t value = */
/*         imm(cpu); */

/*     uint8_t carry = */
/*         cpu->status */
/*         & */
/*         1; */

/*     uint16_t r = */
/*         cpu->a */
/*         + */
/*         value */
/*         + */
/*         carry; */

/*     set_flag( */
/*         cpu, */
/*         FLAG_C, */
/*         r > 0xFF */
/*     ); */

/*     cpu->a = */
/*         r */
/*         & */
/*         0xFF; */

/*     set_flag( */
/*         cpu, */
/*         FLAG_Z, */
/*         cpu->a == 0 */
/*     ); */

/*     set_flag( */
/*         cpu, */
/*         FLAG_N, */
/*         cpu->a & 0x80 */
/*     ); */
/* } */



// =========================
// Transfer Instructions
// =========================

static void update_zn(
    CPU6502* cpu,
    uint8_t value
){
    set_flag(
        cpu,
        Z,
        value == 0
    );

    set_flag(
        cpu,
        N,
        value & 0x80
    );
}


// Load Accumulator

void LDA(
    CPU6502* cpu,
    uint16_t addr
){
    cpu->a =
        mem_read(
            cpu->mem,
            addr
        );

    update_zn(
        cpu,
        cpu->a
    );
}


// Load X

void LDX(
    CPU6502* cpu,
    uint16_t addr
){
    cpu->x =
        mem_read(
            cpu->mem,
            addr
        );

    update_zn(
        cpu,
        cpu->x
    );
}


// Load Y

void LDY(
    CPU6502* cpu,
    uint16_t addr
){
    cpu->y =
        mem_read(
            cpu->mem,
            addr
        );

    update_zn(
        cpu,
        cpu->y
    );
}


// Store Accumulator

void STA(
    CPU6502* cpu,
    uint16_t addr
){
    mem_write(
        cpu->mem,
        addr,
        cpu->a
    );
}


// Store X

void STX(
    CPU6502* cpu,
    uint16_t addr
){
    mem_write(
        cpu->mem,
        addr,
        cpu->x
    );
}


// Store Y

void STY(
    CPU6502* cpu,
    uint16_t addr
){
    mem_write(
        cpu->mem,
        addr,
        cpu->y
    );
}


// Transfer A → X

void TAX(
    CPU6502* cpu
){
    cpu->x =
        cpu->a;

    update_zn(
        cpu,
        cpu->x
    );
}


// Transfer A → Y

void TAY(
    CPU6502* cpu
){
    cpu->y =
        cpu->a;

    update_zn(
        cpu,
        cpu->y
    );
}


// Transfer SP → X

void TSX(
    CPU6502* cpu
){
    cpu->x =
        cpu->sp;

    update_zn(
        cpu,
        cpu->x
    );
}


// Transfer X → A

void TXA(
    CPU6502* cpu
){
    cpu->a =
        cpu->x;

    update_zn(
        cpu,
        cpu->a
    );
}


// Transfer X → SP

void TXS(
    CPU6502* cpu
){
    cpu->sp =
        cpu->x;
}


// Transfer Y → A

void TYA(
    CPU6502* cpu
){
    cpu->a =
        cpu->y;

    update_zn(
        cpu,
        cpu->a
    );
}

// =========================
// Stack Instructions
// =========================

static void update_zn(
    CPU6502* cpu,
    uint8_t value
){
    set_flag(
        cpu,
        Z,
        value == 0
    );

    set_flag(
        cpu,
        N,
        value & 0x80
    );
}


// Push Accumulator

void PHA(
    CPU6502* cpu
){
    push(
        cpu,
        cpu->a
    );
}


// Push Processor Status

void PHP(
    CPU6502* cpu
){
    push(
        cpu,
        cpu->status
        |
        (1 << B)
        |
        (1 << U)
    );
}


// Pull Accumulator

void PLA(
    CPU6502* cpu
){
    cpu->a =
        pop(
            cpu
        );

    update_zn(
        cpu,
        cpu->a
    );
}


// Pull Processor Status

void PLP(
    CPU6502* cpu
){
    cpu->status =
        pop(
            cpu
        );

    cpu->status |=
        (1 << U);
}


// =========================
// Decrements & Increments
// =========================

static void update_zn(
    CPU6502* cpu,
    uint8_t value
){
    set_flag(
        cpu,
        Z,
        value == 0
    );

    set_flag(
        cpu,
        N,
        value & 0x80
    );
}


// Decrement Memory

void DEC(
    CPU6502* cpu,
    uint16_t addr
){
    uint8_t value =
        mem_read(
            cpu->mem,
            addr
        );

    value--;

    mem_write(
        cpu->mem,
        addr,
        value
    );

    update_zn(
        cpu,
        value
    );
}


// Decrement X

void DEX(
    CPU6502* cpu
){
    cpu->x--;

    update_zn(
        cpu,
        cpu->x
    );
}


// Decrement Y

void DEY(
    CPU6502* cpu
){
    cpu->y--;

    update_zn(
        cpu,
        cpu->y
    );
}


// Increment Memory

void INC(
    CPU6502* cpu,
    uint16_t addr
){
    uint8_t value =
        mem_read(
            cpu->mem,
            addr
        );

    value++;

    mem_write(
        cpu->mem,
        addr,
        value
    );

    update_zn(
        cpu,
        value
    );
}


// Increment X

void INX(
    CPU6502* cpu
){
    cpu->x++;

    update_zn(
        cpu,
        cpu->x
    );
}


// Increment Y

void INY(
    CPU6502* cpu
){
    cpu->y++;

    update_zn(
        cpu,
        cpu->y
    );
}



// =========================
// Arithmetic Operations
// =========================

static void update_zn(
    CPU6502* cpu,
    uint8_t value
){
    set_flag(
        cpu,
        Z,
        value == 0
    );

    set_flag(
        cpu,
        N,
        value & 0x80
    );
}


// Add With Carry

void ADC(
    CPU6502* cpu,
    uint16_t addr
){

    uint8_t value =
        mem_read(
            cpu->mem,
            addr
        );

    uint8_t carry =
        (
            cpu->status
            &
            (1 << C)
        )
        ? 1
        : 0;

    uint16_t result =
        cpu->a
        +
        value
        +
        carry;

    set_flag(
        cpu,
        C,
        result > 0xFF
    );

    set_flag(
        cpu,
        V,
        (
            ~(
                cpu->a
                ^
                value
            )
            &
            (
                cpu->a
                ^
                result
            )
            &
            0x80
        )
    );

    cpu->a =
        result
        &
        0xFF;

    update_zn(
        cpu,
        cpu->a
    );
}


/* void ADC( */
/*     CPU6502* cpu, */
/*     uint16_t addr */
/* ){ */
/*     uint8_t value = */
/*         mem_read( */
/*             cpu->mem, */
/*             addr */
/*         ); */

/*     uint8_t carry = */
/*         (cpu->status & (1 << FLAG_C)) */
/*         ? 1 */
/*         : 0; */

/*     // ========================= */
/*     // DECIMAL MODE (BCD) */
/*     // ========================= */
/*     if (cpu->status & (1 << FLAG_D)) */
/*     { */
/*         uint8_t lo = */
/*             (cpu->a & 0x0F) */
/*             + (value & 0x0F) */
/*             + carry; */

/*         uint8_t hi = */
/*             (cpu->a >> 4) */
/*             + (value >> 4); */

/*         if (lo > 9) */
/*         { */
/*             lo -= 10; */
/*             hi++; */
/*         } */

/*         set_flag(cpu, FLAG_C, hi > 9); */

/*         cpu->a = */
/*             ((hi % 10) << 4) */
/*             | (lo & 0x0F); */

/*         update_zn(cpu, cpu->a); */
/*         return; */
/*     } */

/*     // ========================= */
/*     // BINARY MODE (NORMAL 6502) */
/*     // ========================= */
/*     uint16_t result = */
/*         cpu->a */
/*         + value */
/*         + carry; */

/*     set_flag( */
/*         cpu, */
/*         FLAG_C, */
/*         result > 0xFF */
/*     ); */

/*     set_flag( */
/*         cpu, */
/*         FLAG_V, */
/*         (~(cpu->a ^ value) & (cpu->a ^ result) & 0x80) */
/*     ); */

/*     cpu->a = result & 0xFF; */

/*     update_zn(cpu, cpu->a); */
/* } */


// Subtract With Carry

void SBC(
    CPU6502* cpu,
    uint16_t addr
){

    uint8_t value =
        mem_read(
            cpu->mem,
            addr
        );

    value ^= 0xFF;

    uint8_t carry =
        (
            cpu->status
            &
            (1 << C)
        )
        ? 1
        : 0;

    uint16_t result =
        cpu->a
        +
        value
        +
        carry;

    set_flag(
        cpu,
        C,
        result > 0xFF
    );

    set_flag(
        cpu,
        V,
        (
            ~(
                cpu->a
                ^
                value
            )
            &
            (
                cpu->a
                ^
                result
            )
            &
            0x80
        )
    );

    cpu->a =
        result
        &
        0xFF;

    update_zn(
        cpu,
        cpu->a
    );
}


// =========================
// Logical Operations
// =========================

static void update_zn(
    CPU6502* cpu,
    uint8_t value
){
    set_flag(
        cpu,
        Z,
        value == 0
    );

    set_flag(
        cpu,
        N,
        value & 0x80
    );
}


// AND
// A = A & M

void AND(
    CPU6502* cpu,
    uint16_t addr
){

    cpu->a &=
        mem_read(
            cpu->mem,
            addr
        );

    update_zn(
        cpu,
        cpu->a
    );
}


// EOR (XOR)
// A = A ^ M

void EOR(
    CPU6502* cpu,
    uint16_t addr
){

    cpu->a ^=
        mem_read(
            cpu->mem,
            addr
        );

    update_zn(
        cpu,
        cpu->a
    );
}


// ORA (OR)
// A = A | M

void ORA(
    CPU6502* cpu,
    uint16_t addr
){

    cpu->a |=
        mem_read(
            cpu->mem,
            addr
        );

    update_zn(
        cpu,
        cpu->a
    );
}



static void update_zn(CPU6502* cpu, uint8_t v)
{
    set_flag(cpu, Z, v == 0);
    set_flag(cpu, N, v & 0x80);
}

// shift & rotate instructions
void ASL(CPU6502* cpu, uint16_t addr)
{
    uint8_t value = mem_read(cpu->mem, addr);

    set_flag(cpu, C, value & 0x80);

    value <<= 1;

    mem_write(cpu->mem, addr, value);

    update_zn(cpu, value);
}


/* void ASL(CPU6502* cpu, uint16_t addr) */
/* { */
/*     uint8_t value; */

/*     if (addr == 0) // ACC mode */
/*         value = cpu->a; */
/*     else */
/*         value = read_mem(cpu, addr); */

/*     set_flag(cpu, FLAG_C, value & 0x80); */

/*     value <<= 1; */

/*     if (addr == 0) */
/*         cpu->a = value; */
/*     else */
/*         write_mem(cpu, addr, value); */

/*     update_zn(cpu, value); */
/* } */


void ASL_A(CPU6502* cpu)
{
    set_flag(cpu, C, cpu->a & 0x80);

    cpu->a <<= 1;

    update_zn(cpu, cpu->a);
}

void LSR(CPU6502* cpu, uint16_t addr)
{
    uint8_t value = mem_read(cpu->mem, addr);

    set_flag(cpu, C, value & 0x01);

    value >>= 1;

    mem_write(cpu->mem, addr, value);

    update_zn(cpu, value);
}

void LSR_A(CPU6502* cpu)
{
    set_flag(cpu, C, cpu->a & 0x01);

    cpu->a >>= 1;

    update_zn(cpu, cpu->a);
}

void ROL(CPU6502* cpu, uint16_t addr)
{
    uint8_t value = mem_read(cpu->mem, addr);

    uint8_t old_c = (cpu->status & (1 << C)) ? 1 : 0;

    set_flag(cpu, C, value & 0x80);

    value = (value << 1) | old_c;

    mem_write(cpu->mem, addr, value);

    update_zn(cpu, value);
}

void ROL_A(CPU6502* cpu)
{
    uint8_t old_c = (cpu->status & (1 << C)) ? 1 : 0;

    set_flag(cpu, C, cpu->a & 0x80);

    cpu->a = (cpu->a << 1) | old_c;

    update_zn(cpu, cpu->a);
}

void ROR(CPU6502* cpu, uint16_t addr)
{
    uint8_t value = mem_read(cpu->mem, addr);

    uint8_t old_c = (cpu->status & (1 << C)) ? 1 : 0;

    set_flag(cpu, C, value & 0x01);

    value = (value >> 1) | (old_c << 7);

    mem_write(cpu->mem, addr, value);

    update_zn(cpu, value);
}

void ROR_A(CPU6502* cpu)
{
    uint8_t old_c = (cpu->status & (1 << C)) ? 1 : 0;

    set_flag(cpu, C, cpu->a & 0x01);

    cpu->a = (cpu->a >> 1) | (old_c << 7);

    update_zn(cpu, cpu->a);
}

// =========================
// Flag Instructions
// =========================

void CLC(CPU6502* cpu)
{
    set_flag(cpu, C, false);
}

void CLD(CPU6502* cpu)
{
    set_flag(cpu, D, false);
}

void CLI(CPU6502* cpu)
{
    set_flag(cpu, I, false);
}

void CLV(CPU6502* cpu)
{
    set_flag(cpu, V, false);
}

void SEC(CPU6502* cpu)
{
    set_flag(cpu, C, true);
}

void SED(CPU6502* cpu)
{
    set_flag(cpu, D, true);
}

void SEI(CPU6502* cpu)
{
    set_flag(cpu, I, true);
}


// =========================
// Comparison Instructions
// =========================

static void update_cmp_flags(
    CPU6502* cpu,
    uint8_t reg,
    uint8_t value
){
    uint16_t result =
        (uint16_t)reg - value;

    set_flag(cpu, C, reg >= value);
    set_flag(cpu, Z, reg == value);
    set_flag(cpu, N, result & 0x80);
}


// Compare A

void CMP(
    CPU6502* cpu,
    uint16_t addr
){
    uint8_t value =
        mem_read(cpu->mem, addr);

    update_cmp_flags(
        cpu,
        cpu->a,
        value
    );
}


// Compare X

void CPX(
    CPU6502* cpu,
    uint16_t addr
){
    uint8_t value =
        mem_read(cpu->mem, addr);

    update_cmp_flags(
        cpu,
        cpu->x,
        value
    );
}


// Compare Y

void CPY(
    CPU6502* cpu,
    uint16_t addr
){
    uint8_t value =
        mem_read(cpu->mem, addr);

    update_cmp_flags(
        cpu,
        cpu->y,
        value
    );
}

// =========================
// BIT Instruction
// =========================

void BIT(
    CPU6502* cpu,
    uint16_t addr
){
    uint8_t value =
        mem_read(cpu->mem, addr);

    uint8_t result =
        cpu->a & value;

    // Z flag: result == 0
    set_flag(cpu, Z, result == 0);

    // N flag: bit 7 of memory
    set_flag(cpu, N, value & 0x80);

    // V flag: bit 6 of memory
    set_flag(cpu, V, value & 0x40);
}

// conditional branch instructions

static void branch(
    CPU6502* cpu,
    bool condition
){
    int8_t offset =
        (int8_t)mem_read(
            cpu->mem,
            cpu->pc++
        );

    if (condition)
    {
        cpu->pc =
            cpu->pc + offset;
    }
}


void IRQ(CPU6502* cpu)
{
    if (cpu->status & (1 << FLAG_I))
        return;

    push(cpu, (cpu->pc >> 8) & 0xFF);
    push(cpu, cpu->pc & 0xFF);

    push(cpu, cpu->status);

    set_flag(cpu, FLAG_I, true);

    uint16_t lo = read_mem(cpu, 0xFFFE);
    uint16_t hi = read_mem(cpu, 0xFFFF);

    cpu->pc = (hi << 8) | lo;
}


void NMI(CPU6502* cpu)
{
    push(cpu, (cpu->pc >> 8) & 0xFF);
    push(cpu, cpu->pc & 0xFF);

    push(cpu, cpu->status);

    set_flag(cpu, FLAG_I, true);

    uint16_t lo = read_mem(cpu, 0xFFFA);
    uint16_t hi = read_mem(cpu, 0xFFFB);

    cpu->pc = (hi << 8) | lo;
}

/* static void branch(CPU6502* cpu, bool condition) */
/* { */
/*     int8_t offset = (int8_t)read_mem(cpu, cpu->pc++); */

/*     if (condition) */
/*     { */
/*         uint16_t old_pc = cpu->pc; */
/*         cpu->pc = old_pc + offset; */
/*     } */
/* } */


// =========================
// Conditional Branches
// =========================

void BCC(CPU6502* cpu)
{
    branch(
        cpu,
        !(cpu->status & (1 << C))
    );
}

void BCS(CPU6502* cpu)
{
    branch(
        cpu,
        (cpu->status & (1 << C))
    );
}

void BEQ(CPU6502* cpu)
{
    branch(
        cpu,
        (cpu->status & (1 << Z))
    );
}

void BMI(CPU6502* cpu)
{
    branch(
        cpu,
        (cpu->status & (1 << N))
    );
}

void BNE(CPU6502* cpu)
{
    branch(
        cpu,
        !(cpu->status & (1 << Z))
    );
}

void BPL(CPU6502* cpu)
{
    branch(
        cpu,
        !(cpu->status & (1 << N))
    );
}

void BVC(CPU6502* cpu)
{
    branch(
        cpu,
        !(cpu->status & (1 << V))
    );
}

void BVS(CPU6502* cpu)
{
    branch(
        cpu,
        (cpu->status & (1 << V))
    );
}


// =========================
// Jumps & Subroutines
// =========================


// JMP (absolute)

void JMP(
    CPU6502* cpu,
    uint16_t addr
){
    cpu->pc = addr;
}


// JSR (Jump to Subroutine)

void JSR(
    CPU6502* cpu,
    uint16_t addr
){
    uint16_t return_addr =
        cpu->pc - 1;

    // push high byte
    push(cpu, (return_addr >> 8) & 0xFF);

    // push low byte
    push(cpu, return_addr & 0xFF);

    cpu->pc = addr;
}


// RTS (Return from Subroutine)

void RTS(
    CPU6502* cpu
){
    uint8_t lo = pop(cpu);
    uint8_t hi = pop(cpu);

    cpu->pc =
        ((hi << 8) | lo)
        + 1;
}
// =========================
// Interrupts
// =========================

void BRK(
    CPU6502* cpu
){
    cpu->pc++;

    // push PC high
    push(cpu, (cpu->pc >> 8) & 0xFF);

    // push PC low
    push(cpu, cpu->pc & 0xFF);

    // push status with B + U set
    push(
        cpu,
        cpu->status
        | (1 << B)
        | (1 << U)
    );

    // set interrupt disable
    set_flag(cpu, I, true);

    // load interrupt vector
    uint16_t lo =
        mem_read(cpu->mem, 0xFFFE);

    uint16_t hi =
        mem_read(cpu->mem, 0xFFFF);

    cpu->pc =
        (hi << 8) | lo;
}

// =========================
// Other Instructions
// =========================

void NOP(
    CPU6502* cpu
){
    // nothing happens
}

/*=========================
CPU
=========================*/
void init_opcodes(
    CPU6502* cpu
){
    memset(cpu->op, 0, sizeof(cpu->op));

    // =========================
    // Load / Store
    // =========================
    cpu->op[0xA9] = lda_imm;
    cpu->op[0xA5] = lda_zp;
    cpu->op[0xAD] = lda_abs;

    cpu->op[0xA2] = ldx_imm;
    cpu->op[0xA6] = ldx_zp;
    cpu->op[0xAE] = ldx_abs;

    cpu->op[0xA0] = ldy_imm;
    cpu->op[0xA4] = ldy_zp;
    cpu->op[0xAC] = ldy_abs;

    cpu->op[0x85] = sta_zp;
    cpu->op[0x8D] = sta_abs;

    cpu->op[0x86] = stx_zp;
    cpu->op[0x8E] = stx_abs;

    cpu->op[0x84] = sty_zp;
    cpu->op[0x8C] = sty_abs;

    // =========================
    // Transfers
    // =========================
    cpu->op[0xAA] = tax;
    cpu->op[0xA8] = tay;
    cpu->op[0xBA] = tsx;
    cpu->op[0x8A] = txa;
    cpu->op[0x9A] = txs;
    cpu->op[0x98] = tya;

    // =========================
    // Stack
    // =========================
    cpu->op[0x48] = pha;
    cpu->op[0x08] = php;
    cpu->op[0x68] = pla;
    cpu->op[0x28] = plp;

    // =========================
    // Logic
    // =========================
    cpu->op[0x29] = and_imm;
    cpu->op[0x25] = and_zp;
    cpu->op[0x2D] = and_abs;

    cpu->op[0x49] = eor_imm;
    cpu->op[0x45] = eor_zp;
    cpu->op[0x4D] = eor_abs;

    cpu->op[0x09] = ora_imm;
    cpu->op[0x05] = ora_zp;
    cpu->op[0x0D] = ora_abs;

    // =========================
    // Arithmetic
    // =========================
    cpu->op[0x69] = adc_imm;
    cpu->op[0x65] = adc_zp;
    cpu->op[0x6D] = adc_abs;

    cpu->op[0xE9] = sbc_imm;
    cpu->op[0xE5] = sbc_zp;
    cpu->op[0xED] = sbc_abs;

    // =========================
    // Increments / Decrements
    // =========================
    cpu->op[0xE6] = inc_zp;
    cpu->op[0xEE] = inc_abs;

    cpu->op[0xC6] = dec_zp;
    cpu->op[0xCE] = dec_abs;

    cpu->op[0xE8] = inx;
    cpu->op[0xCA] = dex;

    cpu->op[0xC8] = iny;
    cpu->op[0x88] = dey;

    // =========================
    // Shifts & Rotates
    // =========================
    cpu->op[0x0A] = asl_a;
    cpu->op[0x06] = asl_zp;
    cpu->op[0x0E] = asl_abs;

    cpu->op[0x4A] = lsr_a;
    cpu->op[0x46] = lsr_zp;
    cpu->op[0x4E] = lsr_abs;

    cpu->op[0x2A] = rol_a;
    cpu->op[0x26] = rol_zp;
    cpu->op[0x2E] = rol_abs;

    cpu->op[0x6A] = ror_a;
    cpu->op[0x66] = ror_zp;
    cpu->op[0x6E] = ror_abs;

    // =========================
    // Compare
    // =========================
    cpu->op[0xC9] = cmp_imm;
    cpu->op[0xC5] = cmp_zp;
    cpu->op[0xCD] = cmp_abs;

    cpu->op[0xE0] = cpx_imm;
    cpu->op[0xE4] = cpx_zp;
    cpu->op[0xEC] = cpx_abs;

    cpu->op[0xC0] = cpy_imm;
    cpu->op[0xC4] = cpy_zp;
    cpu->op[0xCC] = cpy_abs;

    // =========================
    // Bit test
    // =========================
    cpu->op[0x24] = bit_zp;
    cpu->op[0x2C] = bit_abs;

    // =========================
    // Branches
    // =========================
    cpu->op[0x90] = bcc;
    cpu->op[0xB0] = bcs;
    cpu->op[0xF0] = beq;
    cpu->op[0x30] = bmi;
    cpu->op[0xD0] = bne;
    cpu->op[0x10] = bpl;
    cpu->op[0x50] = bvc;
    cpu->op[0x70] = bvs;

    // =========================
    // Jumps & Subroutines
    // =========================
    cpu->op[0x4C] = jmp_abs;
    cpu->op[0x6C] = jmp_ind;

    cpu->op[0x20] = jsr_abs;
    cpu->op[0x60] = rts;

    // =========================
    // Flags
    // =========================
    cpu->op[0x18] = clc;
    cpu->op[0xD8] = cld;
    cpu->op[0x58] = cli;
    cpu->op[0xB8] = clv;

    cpu->op[0x38] = sec;
    cpu->op[0xF8] = sed;
    cpu->op[0x78] = sei;

    // =========================
    // System
    // =========================
    cpu->op[0x00] = brk;
    cpu->op[0x40] = rti;
    cpu->op[0xEA] = nop;
}

void cpu_reset(
    CPU6502* cpu
) {

    cpu->a=0;
    cpu->x=0;
    cpu->y=0;

    cpu->pc=0;

    cpu->sp=0xFD;

    cpu->status=0;

    set_flag(
        cpu,
        FLAG_I,
        true
    );

    cpu->pc =
        read_word(
            cpu,
            0xFFFC
        );

    cpu->running =
        true;

    cpu->cycles=0;
}

void cpu_step(
    CPU6502* cpu
) {

    uint8_t opcode =
        fetch(cpu);

    OpcodeFn fn =
        cpu->op[opcode];

    if (!fn) {

        fprintf(
            stderr,
            "Unknown opcode 0x%02X\n",
            opcode
        );

        exit(1);
    }

    fn(cpu);
}

void cpu_run(
    CPU6502* cpu
) {

    while (
        cpu->running
    )
        cpu_step(cpu);
    
}


void step(CPU6502* cpu)
{
    uint8_t opcode = read_mem(cpu, cpu->pc++);

    Opcode op = cpu->op[opcode];

    if (!op.fn)
        return; // NOP or invalid

    op.fn(cpu, op.mode);
}




static uint8_t read_mem(CPU6502* cpu, uint16_t addr)
{
    return cpu->mem->data[addr];
}

static void write_mem(CPU6502* cpu, uint16_t addr, uint8_t v)
{
    cpu->mem->data[addr] = v;
}



static uint16_t get_addr(CPU6502* cpu, AddrMode mode)
{
    switch (mode)
    {
        case IMM:
            return cpu->pc++;

        case ZP:
            return read_mem(cpu, cpu->pc++);

        case ABS:
        {
            uint8_t lo = read_mem(cpu, cpu->pc++);
            uint8_t hi = read_mem(cpu, cpu->pc++);
            return (hi << 8) | lo;
        }

        case ZPX:
        {
            uint8_t addr = read_mem(cpu, cpu->pc++);
            return (addr + cpu->x) & 0xFF;
        }

        case ZPY:
        {
            uint8_t addr = read_mem(cpu, cpu->pc++);
            return (addr + cpu->y) & 0xFF;
        }

        case ABX:
        {
            uint8_t lo = read_mem(cpu, cpu->pc++);
            uint8_t hi = read_mem(cpu, cpu->pc++);
            return ((hi << 8) | lo) + cpu->x;
        }

        case ABY:
        {
            uint8_t lo = read_mem(cpu, cpu->pc++);
            uint8_t hi = read_mem(cpu, cpu->pc++);
            return ((hi << 8) | lo) + cpu->y;
        }

        case IND:
        {
            uint8_t lo = read_mem(cpu, cpu->pc++);
            uint8_t hi = read_mem(cpu, cpu->pc++);
            uint16_t ptr = (hi << 8) | lo;

            // 6502 BUG (важно!)
            uint8_t l = read_mem(cpu, ptr);
            uint8_t h = read_mem(cpu, (ptr & 0xFF00) | ((ptr + 1) & 0xFF));

            return (h << 8) | l;
        }

        case IMP:
        default:
            return 0;

      case ZPX:
        {
            uint8_t addr = read_mem(cpu, cpu->pc++);
            return (addr + cpu->x) & 0xFF;
        }

    case ZPY:
        {
            uint8_t addr = read_mem(cpu, cpu->pc++);
            return (addr + cpu->y) & 0xFF;
        }

    case IZX:
        {
            uint8_t zp = read_mem(cpu, cpu->pc++);
            uint8_t ptr = (zp + cpu->x) & 0xFF;

            uint8_t lo = read_mem(cpu, ptr);
            uint8_t hi = read_mem((ptr + 1) & 0xFF);

            return (hi << 8) | lo;
        }

    case IZY:
        {
            uint8_t zp = read_mem(cpu, cpu->pc++);

            uint8_t lo = read_mem(zp);
            uint8_t hi = read_mem((zp + 1) & 0xFF);

            uint16_t base = (hi << 8) | lo;

            return base + cpu->y;
        }
        

    case REL:
        {
            int8_t offset = (int8_t)read_mem(cpu, cpu->pc++);
            return cpu->pc + offset;
        }


    case ACC:
        {
            return 0; // special-case handled in opcode fn
        }
        
        
    }
    


    
}


/* void LDA(CPU6502* cpu, AddrMode mode) */
/* { */
/*     uint16_t addr = get_addr(cpu, mode); */

/*     cpu->a = read_mem(cpu, addr); */

/*     set_flag(cpu, Z, cpu->a == 0); */
/*     set_flag(cpu, N, cpu->a & 0x80); */
/* } */

/*=========================
MAIN
=========================*/

// int main(
//     int argc,
//     char** argv
// ) {
//
//     if (argc < 2) {
//
//         puts(
//             "usage: emu file.bin"
//         );
//
//         return 1;
//     }
//
//     FILE* f =
//         fopen(
//             argv[1],
//             "rb"
//         );
//
//     if (!f) {
//
//         puts(
//             "cannot open file"
//         );
//
//         return 1;
//     }
//
//     fseek(
//         f,
//         0,
//         SEEK_END
//     );
//
//     long size =
//         ftell(f);
//
//     rewind(f);
//
//     uint8_t* blob =
//         malloc(size);
//
//     fread(
//         blob,
//         1,
//         size,
//         f
//     );
//
//     fclose(f);
//
//     Memory mem =
//         {0};
//
//     mem_load(
//         &mem,
//         0x8000,
//         blob,
//         size
//     );
//
//     free(blob);
//
//     mem_write(
//         &mem,
//         0xFFFC,
//         0x00
//     );
//
//     mem_write(
//         &mem,
//         0xFFFD,
//         0x80
//     );
//
//     CPU6502 cpu =
//         {
//             .mem=&mem
//         };
//
//     init_opcodes(
//         &cpu
//     );
//
//     cpu_reset(
//         &cpu
//     );
//
//     cpu_run(
//         &cpu
//     );
//
//     printf(
//         "\nExecuted %llu cycles\n",
//         cpu.cycles
//     );
//
//     return 0;
// }




































































/* /\* */
/* Minimal MOS-6502 CPU MVP emulator (clean single-file version) */
/* *\/ */

/* #include <stdio.h> */
/* #include <stdlib.h> */
/* #include <stdint.h> */
/* #include <stdbool.h> */
/* #include <string.h> */

/* #define MEM_SIZE 0x10000 */

/* /\* ========================= */
/*    FLAGS */
/* ========================= *\/ */

/* typedef enum { */
/*     C = 0, */
/*     Z, */
/*     I, */
/*     D, */
/*     B, */
/*     U, */
/*     V, */
/*     N */
/* } Flag; */

/* /\* ========================= */
/*    ADDRESSING MODES */
/* ========================= *\/ */

/* typedef enum { */
/*     IMM, */
/*     ZP, */
/*     ABS, */
/*     IND, */
/*     ZPX, */
/*     ZPY, */
/*     ABX, */
/*     ABY, */
/*     IMP, */
/*     REL, */
/*     ACC */
/* } AddrMode; */

/* /\* ========================= */
/*    MEMORY */
/* ========================= *\/ */

/* typedef struct { */
/*     uint8_t data[MEM_SIZE]; */
/* } Memory; */

/* /\* ========================= */
/*    CPU */
/* ========================= *\/ */

/* typedef struct CPU6502 CPU6502; */

/* typedef void (*InstrFn)(CPU6502*, AddrMode); */

/* typedef struct { */
/*     InstrFn fn; */
/*     AddrMode mode; */
/* } Opcode; */

/* struct CPU6502 { */
/*     uint8_t a, x, y; */
/*     uint8_t sp; */
/*     uint8_t status; */
/*     uint16_t pc; */

/*     Memory* mem; */

/*     Opcode op[256]; */

/*     bool running; */
/*     uint64_t cycles; */
/* }; */

/* /\* ========================= */
/*    MEMORY ACCESS */
/* ========================= *\/ */

/* static void mem_check(uint16_t addr) { */
/*     (void)addr; */
/* } */

/* static uint8_t mem_read(Memory* mem, uint16_t addr) { */
/*     mem_check(addr); */
/*     return mem->data[addr]; */
/* } */

/* static void mem_write(Memory* mem, uint16_t addr, uint8_t v) { */
/*     mem_check(addr); */
/*     mem->data[addr] = v; */
/* } */

/* /\* ========================= */
/*    FLAGS HELPERS */
/* ========================= *\/ */

/* static void set_flag(CPU6502* cpu, Flag f, bool v) { */
/*     if (v) cpu->status |= (1 << f); */
/*     else cpu->status &= ~(1 << f); */
/* } */

/* static bool get_flag(CPU6502* cpu, Flag f) { */
/*     return cpu->status & (1 << f); */
/* } */

/* static void update_zn(CPU6502* cpu, uint8_t v) { */
/*     set_flag(cpu, Z, v == 0); */
/*     set_flag(cpu, N, v & 0x80); */
/* } */

/* /\* ========================= */
/*    STACK */
/* ========================= *\/ */

/* static void push(CPU6502* cpu, uint8_t v) { */
/*     mem_write(cpu->mem, 0x100 + cpu->sp, v); */
/*     cpu->sp--; */
/* } */

/* static uint8_t pop(CPU6502* cpu) { */
/*     cpu->sp++; */
/*     return mem_read(cpu->mem, 0x100 + cpu->sp); */
/* } */

/* /\* ========================= */
/*    FETCH */
/* ========================= *\/ */

/* static uint8_t fetch(CPU6502* cpu) { */
/*     return mem_read(cpu->mem, cpu->pc++); */
/* } */

/* static uint16_t fetch_word(CPU6502* cpu) { */
/*     uint8_t lo = fetch(cpu); */
/*     uint8_t hi = fetch(cpu); */
/*     return (hi << 8) | lo; */
/* } */

/* /\* ========================= */
/*    ADDRESSING */
/* ========================= *\/ */

/* static uint16_t get_addr(CPU6502* cpu, AddrMode mode) { */
/*     switch (mode) { */

/*         case IMM: */
/*             return cpu->pc++; */

/*         case ZP: */
/*             return fetch(cpu); */

/*         case ABS: */
/*             return fetch_word(cpu); */

/*         case ZPX: */
/*             return (fetch(cpu) + cpu->x) & 0xFF; */

/*         case ZPY: */
/*             return (fetch(cpu) + cpu->y) & 0xFF; */

/*         case ABX: */
/*             return fetch_word(cpu) + cpu->x; */

/*         case ABY: */
/*             return fetch_word(cpu) + cpu->y; */

/*         case IND: { */
/*             uint16_t ptr = fetch_word(cpu); */
/*             uint8_t lo = mem_read(cpu->mem, ptr); */
/*             uint8_t hi = mem_read(cpu->mem, (ptr & 0xFF00) | ((ptr + 1) & 0xFF)); */
/*             return (hi << 8) | lo; */
/*         } */

/*         case REL: { */
/*             int8_t off = (int8_t)fetch(cpu); */
/*             return cpu->pc + off; */
/*         } */

/*         default: */
/*             return 0; */
/*     } */
/* } */

/* /\* ========================= */
/*    LOAD / STORE */
/* ========================= *\/ */

/* static void LDA(CPU6502* c, AddrMode m) { */
/*     c->a = mem_read(c->mem, get_addr(c, m)); */
/*     update_zn(c, c->a); */
/* } */

/* static void LDX(CPU6502* c, AddrMode m) { */
/*     c->x = mem_read(c->mem, get_addr(c, m)); */
/*     update_zn(c, c->x); */
/* } */

/* static void LDY(CPU6502* c, AddrMode m) { */
/*     c->y = mem_read(c->mem, get_addr(c, m)); */
/*     update_zn(c, c->y); */
/* } */

/* static void STA(CPU6502* c, AddrMode m) { */
/*     mem_write(c->mem, get_addr(c, m), c->a); */
/* } */

/* /\* ========================= */
/*    TRANSFERS */
/* ========================= *\/ */

/* static void TAX(CPU6502* c, AddrMode m){ (void)m; c->x = c->a; update_zn(c,c->x); } */
/* static void TAY(CPU6502* c, AddrMode m){ (void)m; c->y = c->a; update_zn(c,c->y); } */
/* static void TXA(CPU6502* c, AddrMode m){ (void)m; c->a = c->x; update_zn(c,c->a); } */
/* static void TYA(CPU6502* c, AddrMode m){ (void)m; c->a = c->y; update_zn(c,c->a); } */

/* /\* ========================= */
/*    ARITHMETIC */
/* ========================= *\/ */

/* static void ADC(CPU6502* c, AddrMode m) { */
/*     uint8_t v = mem_read(c->mem, get_addr(c, m)); */
/*     uint16_t r = c->a + v + get_flag(c, C); */

/*     set_flag(c, C, r > 0xFF); */
/*     c->a = r & 0xFF; */
/*     update_zn(c, c->a); */
/* } */

/* /\* ========================= */
/*    BRANCH */
/* ========================= *\/ */

/* static void branch(CPU6502* c, bool cond) { */
/*     int8_t off = (int8_t)fetch(c); */
/*     if (cond) c->pc += off; */
/* } */

/* static void BEQ(CPU6502* c, AddrMode m){ (void)m; branch(c, get_flag(c,Z)); } */
/* static void BNE(CPU6502* c, AddrMode m){ (void)m; branch(c, !get_flag(c,Z)); } */
/* static void BCS(CPU6502* c, AddrMode m){ (void)m; branch(c, get_flag(c,C)); } */
/* static void BCC(CPU6502* c, AddrMode m){ (void)m; branch(c, !get_flag(c,C)); } */

/* /\* ========================= */
/*    JUMP / CALL */
/* ========================= *\/ */

/* static void JMP(CPU6502* c, AddrMode m) { */
/*     c->pc = get_addr(c, m); */
/* } */

/* static void JSR(CPU6502* c, AddrMode m) { */
/*     uint16_t a = get_addr(c, m); */
/*     push(c, (c->pc - 1) >> 8); */
/*     push(c, (c->pc - 1) & 0xFF); */
/*     c->pc = a; */
/* } */

/* static void RTS(CPU6502* c, AddrMode m) { */
/*     (void)m; */
/*     uint8_t lo = pop(c); */
/*     uint8_t hi = pop(c); */
/*     c->pc = ((hi << 8) | lo) + 1; */
/* } */

/* /\* ========================= */
/*    SYSTEM */
/* ========================= *\/ */

/* static void NOP(CPU6502* c, AddrMode m){ (void)c; (void)m; } */

/* static void BRK(CPU6502* c, AddrMode m){ */
/*     (void)m; */
/*     c->running = false; */
/* } */

/* /\* ========================= */
/*    OPCODE TABLE */
/* ========================= *\/ */

/* static void init_opcodes(CPU6502* c) { */
/*     memset(c->op, 0, sizeof(c->op)); */

/*     #define OP(code, fn, mode) c->op[code] = (Opcode){fn, mode} */

/*     OP(0xA9, LDA, IMM); */
/*     OP(0xA5, LDA, ZP); */
/*     OP(0xAD, LDA, ABS); */

/*     OP(0xAA, TAX, IMP); */
/*     OP(0xA8, TAY, IMP); */
/*     OP(0x8A, TXA, IMP); */
/*     OP(0x98, TYA, IMP); */

/*     OP(0x69, ADC, IMM); */

/*     OP(0xD0, BNE, REL); */
/*     OP(0xF0, BEQ, REL); */
/*     OP(0xB0, BCS, REL); */
/*     OP(0x90, BCC, REL); */

/*     OP(0x4C, JMP, ABS); */
/*     OP(0x20, JSR, ABS); */
/*     OP(0x60, RTS, IMP); */

/*     OP(0xEA, NOP, IMP); */
/*     OP(0x00, BRK, IMP); */

/*     #undef OP */
/* } */

/* /\* ========================= */
/*    CPU CORE */
/* ========================= *\/ */

/* static void step(CPU6502* c) { */
/*     uint8_t op = fetch(c); */
/*     Opcode ins = c->op[op]; */

/*     if (!ins.fn) { */
/*         printf("Unknown opcode %02X\n", op); */
/*         c->running = false; */
/*         return; */
/*     } */

/*     ins.fn(c, ins.mode); */
/* } */

/* static void run(CPU6502* c) { */
/*     while (c->running) step(c); */
/* } */

/* /\* ========================= */
/*    RESET */
/* ========================= *\/ */

/* static void reset(CPU6502* c) { */
/*     c->a = c->x = c->y = 0; */
/*     c->sp = 0xFD; */
/*     c->status = 0; */
/*     c->cycles = 0; */
/*     c->running = true; */

/*     c->pc = mem_read(c->mem, 0xFFFC) */
/*           | (mem_read(c->mem, 0xFFFD) << 8); */
/* } */

/* /\* ========================= */
/*    MAIN (optional) */
/* ========================= *\/ */

/* int main() { */
/*     Memory mem = {0}; */

/*     CPU6502 cpu = { */
/*         .mem = &mem */
/*     }; */

/*     init_opcodes(&cpu); */
/*     reset(&cpu); */

/*     run(&cpu); */

/*     return 0; */
/* } */
