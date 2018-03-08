#include "cpu.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "logging.h"

static void pflags (const int level, const struct flags f) {
  if (level <= LOG_LEVEL) {
    printf("Flags (Z N H C) (%u %u %u %u)\n", f.z, f.n, f.h, f.c);
  }
}

static uint16_t load_d16 (const struct cpu* const lr35902) {
  uint16_t pc = lr35902->registers.pc;
  return rw(lr35902->mmu, pc + 1);
}

static uint8_t load_d8 (const struct cpu* const lr35902) {
  uint16_t pc = lr35902->registers.pc;
  return rb(lr35902->mmu, pc + 1);
}

static int8_t load_r8 (const struct cpu* const cpu) {
  // r8 are relative values and should be interpreted as signed.
  return (int8_t) load_d8(cpu);
}

static void store_d16 (const struct cpu* const cpu, const uint16_t addr,
    const uint16_t value) {
  ww(cpu->mmu, addr, value);
}

static void CB_BIT_7_H (struct cpu* const lr35902) {
  LOG(5, "BIT 7,H\n");
  LOG(6, "h: %d\n", lr35902->registers.h);
  LOG(6, "z: %d\n", lr35902->registers.f.z);
  lr35902->registers.f.z = (lr35902->registers.h & (1 << 6)) != 0;
  LOG(6, "z: %d\n", lr35902->registers.f.z);
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 0;
}

static void CB_RL_C (struct cpu* const lr35902) {
  // rotate left aka circular shift
  LOG(5, "RL C\n");
  const uint8_t old_7_bit = (lr35902->registers.c & 0x80) >> 7;
  lr35902->registers.c <<= 1;
  lr35902->registers.c ^=
    (-lr35902->registers.f.c ^ lr35902->registers.c) & (1U << 0);
  lr35902->registers.f.z = !lr35902->registers.c;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 0;
  lr35902->registers.f.c = old_7_bit;
}

static void CB_SWAP_A (struct cpu* const lr35902) {
  LOG(5, "SWAP\n");
  const uint8_t x = lr35902->registers.a;
  lr35902->registers.a = (x & 0x0F) << 4 | (x & 0xF0) >> 4;
  lr35902->registers.f.z = !lr35902->registers.a;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 0;
  lr35902->registers.f.c = 0;
}

static void CB_SRL_A (struct cpu* const lr35902) {
  LOG(5, "SRL A\n");
  const uint8_t old_bit_0 = lr35902->registers.a & 0x01;
  lr35902->registers.a >>= 1;
  lr35902->registers.f.z = !lr35902->registers.a;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 0;
  lr35902->registers.f.c = old_bit_0;
}

static const instr cb_opcodes [256] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x
  0, CB_RL_C, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 1x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 2x
  0, 0, 0, 0, 0, 0, 0, CB_SWAP_A, 0, 0, 0, 0, 0, 0, 0, CB_SRL_A, // 3x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 4x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 5x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 6x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, CB_BIT_7_H, 0, 0, 0, // 7x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 8x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 9x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Ax
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Bx
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Cx
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Dx
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Ex
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Fx
};

instr decode_cb (const struct cpu* const cpu) {
  uint16_t pc = cpu->registers.pc;
  uint8_t opcode = rb(cpu->mmu, pc);
  PBYTE(4, opcode);
  instr i = cb_opcodes[opcode];
#ifndef NDEBUG
  if (i == 0) {
    fprintf(stderr, "Unknown opcode for CB PREFIX " PRIbyte " @ " PRIshort "\n",
        opcode, pc);
  }
#endif
  assert(i != 0);
  return i;
}

static void handle_cb (struct cpu* const cpu) {
  ++cpu->registers.pc;
  instr i = decode_cb(cpu);
  i(cpu);
  ++cpu->registers.pc;
}

// normal instructions (non-cb)
static void NOP (struct cpu* const lr35902) {
  LOG(5, "NOP\n");
  ++lr35902->registers.pc;
}

#define REG(reg) lr35902->registers.reg
// TODO: will only read one byte
#define DEREF_READ(reg) rb(lr35902->mmu, REG(reg))
// TODO: does not compose
#define DEREF_WRITE(reg, val) wb(lr35902->mmu, reg(reg), (val))

#define INC_x(x, X)\
static void INC_ ## X (struct cpu* const lr35902) { \
  LOG(5, "INC " #X "\n"); \
  ++REG(x); \
  lr35902->registers.f.z = lr35902->registers.x == 0; \
  lr35902->registers.f.n = 1; \
  lr35902->registers.f.h = (lr35902->registers.x & 0x10) == 0x10; \
  ++REG(pc); \
}

INC_x(a, A);
INC_x(b, B);
INC_x(c, C);
INC_x(d, D);
INC_x(e, E);
INC_x(h, H);
INC_x(l, L);

#define INC_xy(xy, XY)\
static void INC_ ## XY (struct cpu* const lr35902) { \
  LOG(5, "INC " #XY "\n"); \
  PSHORT(6, REG(xy)); \
  ++REG(xy); \
  PSHORT(6, REG(xy)); \
  ++REG(pc); \
}

INC_xy(bc, BC);
INC_xy(de, DE);
INC_xy(hl, HL);

#define DEC_x(x, X)\
static void DEC_ ## X (struct cpu* const lr35902) {\
  LOG(5, "DEC " #X "\n"); \
  --x; \
  lr35902->registers.f.z = x == 0; \
  lr35902->registers.f.n = 1; \
  lr35902->registers.f.h = (x & 0x10) == 0x10; \
  ++lr35902->registers.pc; \
}

DEC_x(REG(a), A);
DEC_x(REG(b), B);
DEC_x(REG(c), C);
DEC_x(REG(d), D);
DEC_x(REG(e), E);
DEC_x(REG(l), L);

static void DEC_DEREF_HL (struct cpu* const lr35902) {
  LOG(5, "DEC (HL)\n");
  const uint8_t result = rb(lr35902->mmu, lr35902->registers.hl) - 1;
  wb(lr35902->mmu, lr35902->registers.hl, result);
  lr35902->registers.f.z = !result;
  lr35902->registers.f.n = 1;
  lr35902->registers.f.h = (result & 0x10) == 0x10;
  ++lr35902->registers.pc;
}


#define DEC_xy(xy, XY) \
static void DEC_ ## XY (struct cpu* const lr35902) { \
  LOG(5, "DEC " #XY "\n"); \
  PSHORT(6, REG(xy)); \
  --lr35902->registers.xy; \
  PSHORT(6, REG(xy)); \
  ++lr35902->registers.pc; \
}

DEC_xy(bc, BC);

#define XOR_x(x, X) \
static void XOR_ ## X (struct cpu* const lr35902) { \
  LOG(5, "XOR " #X "\n"); \
  lr35902->registers.a ^= lr35902->registers.x; \
  lr35902->registers.f.z = !lr35902->registers.a; \
  lr35902->registers.f.n = 0; \
  lr35902->registers.f.h = 0; \
  lr35902->registers.f.c = 0; \
  ++lr35902->registers.pc; \
}

XOR_x(a, A);
XOR_x(c, C);

#define ADD_A_x(x, X) \
static void ADD_A_ ## X (struct cpu* const lr35902) { \
  LOG(5, "ADD A," #X "\n"); \
  REG(a) += x; \
  lr35902->registers.f.z = lr35902->registers.a == 0; \
  lr35902->registers.f.n = 0; \
  lr35902->registers.f.h = (lr35902->registers.a & 0x10) == 0x10; \
  lr35902->registers.f.c = (lr35902->registers.a & 0x80) != 0; \
  ++REG(pc); \
}

ADD_A_x(REG(a), A);
ADD_A_x(DEREF_READ(hl), DEREF_HL);

// because of the +2 pc
/*ADD_A_x(load_d8(lr35902), d8);*/
static void ADD_A_d8 (struct cpu* const lr35902) {
  LOG(5, "ADD A,d8\n");
  const uint8_t d8 = load_d8(lr35902);
  LOG(6, "where d8 == " PRIbyte "\n", d8);
  lr35902->registers.a += d8;
  lr35902->registers.f.z = lr35902->registers.a == 0;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = (lr35902->registers.a & 0x10) == 0x10;
  lr35902->registers.f.c = (lr35902->registers.a & 0x80) != 0;
  lr35902->registers.pc += 2;
}

#define ADD_xy_zw(xy, zw, XY, ZW) \
static void ADD_ ## XY ## _ ## ZW (struct cpu* const lr35902) { \
  LOG(5, "ADD " #XY "," #ZW "\n"); \
  REG(xy) += REG(zw); \
  lr35902->registers.f.n = 0; \
  lr35902->registers.f.h = (REG(xy) & 0x10) == 0x10; \
  lr35902->registers.f.c = (REG(xy) & 0x80) != 0; \
  ++REG(pc); \
}

ADD_xy_zw(hl, de, HL, DE);
ADD_xy_zw(hl, hl, HL, HL);

static void ADC_A_d8 (struct cpu* const lr35902) {
  LOG(5, "ADC A,d8\n");
  const uint8_t d8 = load_d8(lr35902);
  LOG(6, "where d8 == " PRIbyte "\n", d8);
  lr35902->registers.a += d8 + lr35902->registers.f.c;
  lr35902->registers.f.z = lr35902->registers.a == 0;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = (lr35902->registers.a & 0x10) == 0x10;
  lr35902->registers.f.c = (lr35902->registers.a & 0x80) != 0;
  lr35902->registers.pc += 2;
}

#define SUB_x(x, X) \
static void SUB_ ##X (struct cpu* const lr35902) { \
  LOG(5, "SUB " #X "\n"); \
  REG(a) -= x; \
  lr35902->registers.f.z = lr35902->registers.a == 0; \
  lr35902->registers.f.n = 1; \
  lr35902->registers.f.h = (lr35902->registers.a & 0x10) == 0x10; \
  lr35902->registers.f.c = (lr35902->registers.a & 0x80) != 0; \
  ++REG(pc); \
}

SUB_x(REG(b), B);

// because of the pc += 2
/*SUB_x(load_d8(lr35902), d8);*/
static void SUB_d8 (struct cpu* const lr35902) {
  LOG(5, "SUB d8\n");
  const uint8_t d8 = load_d8(lr35902);
  LOG(6, "where d8 == " PRIbyte "\n", d8);
  lr35902->registers.a -= d8;
  lr35902->registers.f.z = lr35902->registers.a == 0;
  lr35902->registers.f.n = 1;
  lr35902->registers.f.h = (lr35902->registers.a & 0x10) == 0x10;
  lr35902->registers.f.c = (lr35902->registers.a & 0x80) != 0;
  lr35902->registers.pc += 2;
}


#define LD_x_y(x, y, X, Y) \
static void LD_ ## X ## _ ## Y (struct cpu* const lr35902) { \
  LOG(5, "LD " #X "," #Y "\n"); \
  x = y; \
  PBYTE(6, x); \
  ++REG(pc); \
}

LD_x_y(REG(a), REG(b), A, B);
LD_x_y(REG(a), REG(c), A, C);
LD_x_y(REG(a), REG(d), A, D);
LD_x_y(REG(a), REG(e), A, E);
LD_x_y(REG(a), REG(h), A, H);
LD_x_y(REG(a), REG(l), A, L);
LD_x_y(REG(b), REG(a), B, A);
LD_x_y(REG(b), REG(b), B, B);
LD_x_y(REG(c), REG(a), C, A);
LD_x_y(REG(d), REG(a), D, A);
LD_x_y(REG(e), REG(a), E, A);
LD_x_y(REG(h), REG(a), H, A);
LD_x_y(REG(h), REG(b), H, B);
LD_x_y(REG(l), REG(a), L, A);
LD_x_y(REG(sp), REG(hl), SP, HL);
LD_x_y(REG(a), DEREF_READ(de), A, DEREF_DE);
LD_x_y(REG(a), DEREF_READ(hl), A, DEREF_HL);
LD_x_y(REG(d), DEREF_READ(hl), D, DEREF_HL);
LD_x_y(REG(e), DEREF_READ(hl), E, DEREF_HL);
LD_x_y(REG(h), DEREF_READ(hl), H, DEREF_HL);
LD_x_y(REG(l), DEREF_READ(hl), L, DEREF_HL);
/*LD_x_y(DEREF_WRITE(hl), REG(a), DEREF_HL, A);*/

static void LD_DEREF_DE_A (struct cpu* const lr35902) {
  LOG(5, "LD (DE),A\n");

  wb(lr35902->mmu, lr35902->registers.de, lr35902->registers.a);
  ++lr35902->registers.pc;
}

static void LD_DEREF_HL_A (struct cpu* const lr35902) {
  LOG(5, "LD (HL),A\n");

  PBYTE(6, rb(lr35902->mmu, lr35902->registers.hl));
  wb(lr35902->mmu, lr35902->registers.hl, lr35902->registers.a);
  PBYTE(6, rb(lr35902->mmu, lr35902->registers.hl));

  ++lr35902->registers.pc;
}

static void LD_HL_SP_r8 (struct cpu* const lr35902) {
  LOG(5, "LD HL,SP+r8\n");
  const uint8_t r8 = load_r8(lr35902);
  lr35902->registers.hl = lr35902->registers.sp + r8;
  lr35902->registers.f.z = 0;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = (lr35902->registers.sp & 0x10) == 0x10;
  lr35902->registers.f.c = (lr35902->registers.sp & 0x80) != 0;
  lr35902->registers.pc += 2;
}

static void LD_DEREF_HL_DEC_A (struct cpu* const lr35902) {
  LOG(5, "LD (HL-),A\n");

  PBYTE(6, rb(lr35902->mmu, lr35902->registers.hl));
  wb(lr35902->mmu, lr35902->registers.hl, lr35902->registers.a);
  PBYTE(6, rb(lr35902->mmu, lr35902->registers.hl));

  PSHORT(6, lr35902->registers.hl);
  --lr35902->registers.hl;
  PSHORT(6, lr35902->registers.hl);

  ++lr35902->registers.pc;
}

static void LD_DEREF_HL_INC_A (struct cpu* const lr35902) {
  LOG(5, "LD (HL+),A\n");

  PBYTE(6, rb(lr35902->mmu, lr35902->registers.hl));
  wb(lr35902->mmu, lr35902->registers.hl, lr35902->registers.a);
  PBYTE(6, rb(lr35902->mmu, lr35902->registers.hl));

  PSHORT(6, lr35902->registers.hl);
  ++lr35902->registers.hl;
  PSHORT(6, lr35902->registers.hl);

  ++lr35902->registers.pc;
}

static void LD_DEREF_HL_d8 (struct cpu* const lr35902) {
  LOG(5, "LD (HL),d8\n");
  const uint8_t d8 = load_d8(lr35902);
  wb(lr35902->mmu, lr35902->registers.hl, d8);
  lr35902->registers.pc += 2;
}

static void LD_DEREF_C_A (struct cpu* const lr35902) {
  LOG(5, "LD (C),A\n");

  // http://www.chrisantonellis.com/files/gameboy/gb-instructions.txt
  PBYTE(6, rb(lr35902->mmu, lr35902->registers.c + 0xFF00));
  wb(lr35902->mmu, lr35902->registers.c + 0xFF00, lr35902->registers.a);
  PBYTE(6, rb(lr35902->mmu, lr35902->registers.c + 0xFF00));

  // Errata in:
  // http://pastraiser.com/cpu/gameboy/gameboy_opcodes.html
  // instr is actually length 1, not 2
  ++lr35902->registers.pc;
}

static void LDH_DEREF_a8_A (struct cpu* const lr35902) {
  LOG(5, "LDH (a8),A\n");
  uint8_t a8 = load_d8(lr35902);
  LOG(6, "where a8 == ");
  PBYTE(6, a8);

  wb(lr35902->mmu, a8 + 0xFF00, lr35902->registers.a);

  lr35902->registers.pc += 2;
}

static void LDH_A_DEREF_a8 (struct cpu* const lr35902) {
  LOG(5, "LDH A,(a8)\n");
  uint8_t a8 = load_d8(lr35902);
  LOG(6, "where a8 == ");
  PBYTE(6, a8);

  lr35902->registers.a = rb(lr35902->mmu, a8 + 0xFF00);
  lr35902->registers.pc += 2;
}

#define LD_x_d8(x, X) \
static void LD_ ## X ## _d8 (struct cpu* const lr35902) { \
  LOG(5, "LD " #X ",d8\n"); \
  x = load_d8(lr35902); \
  REG(pc) += 2; \
}

LD_x_d8(REG(a), A);
LD_x_d8(REG(b), B);
LD_x_d8(REG(c), C);
LD_x_d8(REG(d), D);
LD_x_d8(REG(e), E);
LD_x_d8(REG(h), H);
LD_x_d8(REG(l), L);

#define LD_xy_d16(xy, XY) \
static void LD_ ## XY ## _d16 (struct cpu* const lr35902) {\
  LOG(5, "LD " #XY ",d16\n"); \
  xy = load_d16(lr35902); \
  REG(pc) += 3; \
}

LD_xy_d16(REG(bc), BC);
LD_xy_d16(REG(de), DE);
LD_xy_d16(REG(hl), HL);
LD_xy_d16(REG(sp), SP);

static void LD_A_DEREF_HL_INC (struct cpu* const lr35902) {
  LOG(5, "LD A,(HL+)\n");

  PBYTE(6, lr35902->registers.a);
  lr35902->registers.a = rb(lr35902->mmu, lr35902->registers.hl);
  PBYTE(6, lr35902->registers.a);

  PSHORT(6, lr35902->registers.hl);
  ++lr35902->registers.hl;
  PSHORT(6, lr35902->registers.hl);

  ++lr35902->registers.pc;
}

static void LD_A_DEREF_a16 (struct cpu* const lr35902) {
  LOG(5, "LD A,(a16)\n");
  const uint16_t a16 = load_d16(lr35902);
  PSHORT(6, a16);
  lr35902->registers.a = rb(lr35902->mmu, a16);
  lr35902->registers.pc += 3;
}

static void LD_DEREF_a16_A (struct cpu* const lr35902) {
  LOG(5, "LD (a16),A\n");
  const uint16_t a16 = load_d16(lr35902);
  PSHORT(6, a16);
  wb(lr35902->mmu, a16, lr35902->registers.a);
  lr35902->registers.pc += 3;
}

static void LD_DEREF_BC_A (struct cpu* const lr35902) {
  LOG(5, "LD (BC),A\n");
  wb(lr35902->mmu, REG(bc), REG(a));
  ++REG(pc);
}

#define PUSH_xy(xy, XY) \
static void PUSH_ ## XY (struct cpu* const lr35902) { \
  LOG(5, "PUSH " #XY "\n"); \
  REG(sp) -= 2; \
  store_d16(lr35902, REG(sp), REG(xy)); \
  ++REG(pc); \
}

PUSH_xy(af, AF);
PUSH_xy(bc, BC);
PUSH_xy(de, DE);
PUSH_xy(hl, HL);

#define POP_xy(xy, XY) \
static void POP_ ## XY (struct cpu* const lr35902) { \
  LOG(5, "POP " #XY "\n"); \
  REG(xy) = rw(lr35902->mmu, REG(sp)); \
  PSHORT(6, REG(xy)); \
  REG(sp) += 2; \
  ++REG(pc); \
}

POP_xy(af, AF);
POP_xy(bc, BC);
POP_xy(de, DE);
POP_xy(hl, HL);

static void __JR_r8 (struct cpu* const lr35902) {
  lr35902->registers.pc += load_r8(lr35902) + 2;
}

static void JR_r8 (struct cpu* const lr35902) {
  LOG(5, "JR r8\n");
  // TODO: remove the double load when not debugging
  int8_t r8 = load_r8(lr35902);
  LOG(6, "where r8 == %d\n", r8);
  PSHORT(6, lr35902->registers.pc);
  __JR_r8(lr35902);
  PSHORT(6, lr35902->registers.pc);
}

static void __JR_COND_r8 (struct cpu* const lr35902, int cond) {
  if (cond) {
    LOG(6, "jumping\n");
    __JR_r8(lr35902);
  } else {
    LOG(6, "not jumping\n");
    lr35902->registers.pc += 2;
  }
}

static void JR_NZ_r8 (struct cpu* const lr35902) {
  LOG(5, "JR NZ,r8\n");
  int8_t r8 = load_r8(lr35902);
  LOG(6, "where r8 == %d\n", r8);
  PSHORT(6, lr35902->registers.pc);

  __JR_COND_r8(lr35902, !lr35902->registers.f.z);
}

static void JR_Z_r8 (struct cpu* const lr35902) {
  LOG(5, "JR Z,r8\n");
  int8_t r8 = load_r8(lr35902);
  LOG(6, "where r8 == %d\n", r8);
  PSHORT(6, lr35902->registers.pc);

  __JR_COND_r8(lr35902, lr35902->registers.f.z);
  PSHORT(6, lr35902->registers.pc);
}

static void JR_NC_r8 (struct cpu* const lr35902) {
  LOG(5, "JR NC,r8\n");
  int8_t r8 = load_r8(lr35902);
  LOG(6, "where r8 == " PRIbyte "\n", r8);
  PSHORT(6, lr35902->registers.pc);

  __JR_COND_r8(lr35902, lr35902->registers.f.c);
  PSHORT(6, lr35902->registers.pc);
}

static void JP_a16 (struct cpu* const lr35902) {
  LOG(5, "JP_a16\n");
  const uint16_t a16 = load_d16(lr35902);
  LOG(6, "jumping to ");
  PSHORT(6, a16);
  lr35902->registers.pc = a16;
}

static void JP_NZ_a16 (struct cpu* const lr35902) {
  LOG(5, "JP NZ,a16\n");
  if (lr35902->registers.f.z) {
    REG(pc) += 3;
  } else {
    REG(pc) = load_d16(lr35902);
  }
}

static void JP_DEREF_HL (struct cpu* const lr35902) {
  LOG(5, "JP (HL)\n");
  lr35902->registers.pc = rw(lr35902->mmu, lr35902->registers.hl);
  LOG(6, "jumping to ");
  PSHORT(6, lr35902->registers.pc);
}

static void CALL_a16 (struct cpu* const lr35902) {
  LOG(5, "CALL a16\n");
  // push address of next instruction onto stack
  lr35902->registers.sp -= 2;
  store_d16(lr35902, lr35902->registers.sp, lr35902->registers.pc + 3);

  // jump to address
  const uint16_t a16 = load_d16(lr35902);
  lr35902->registers.pc = a16;
}

static void CALL_NZ_a16 (struct cpu* const lr35902) {
  LOG(5, "CALL NZ,a16\n");
  // push address of next instruction onto stack
  lr35902->registers.sp -= 2;
  store_d16(lr35902, lr35902->registers.sp, lr35902->registers.pc + 3);

  pflags(6, lr35902->registers.f);
  if (lr35902->registers.f.z) {
    // jump to address
    const uint16_t a16 = load_d16(lr35902);
    lr35902->registers.pc = a16;
    LOG(6, "jumping to " PRIshort "\n", lr35902->registers.pc);
  } else {
    LOG(6, "not jumping\n");
    lr35902->registers.pc += 3;
  }
}

static void RET (struct cpu* const lr35902) {
  LOG(5, "RET\n");
  // pop 2B from stack
  const uint16_t a = rw(lr35902->mmu, lr35902->registers.sp);
  PSHORT(6, a);
  lr35902->registers.sp += 2;
  // jump to that address
  lr35902->registers.pc = a;
}

static void RET_Z (struct cpu* const lr35902) {
  LOG(5, "RET Z\n");
  if (lr35902->registers.f.z) {
    // pop 2B from stack
    lr35902->registers.sp += 2;
    // jump to that address
    LOG(6, "jumping\n");
    lr35902->registers.pc = rw(lr35902->mmu, lr35902->registers.sp);
  } else {
    LOG(6, "not jumping\n");
    ++lr35902->registers.pc;
  }
}

static void RET_C (struct cpu* const lr35902) {
  LOG(5, "RET C\n");
  if (lr35902->registers.f.c) {
    // pop 2B from stack
    lr35902->registers.sp += 2;
    // jump to that address
    LOG(6, "jumping\n");
    lr35902->registers.pc = rw(lr35902->mmu, lr35902->registers.sp);
  } else {
    LOG(6, "not jumping\n");
    ++lr35902->registers.pc;
  }
}

static void RET_NC (struct cpu* const lr35902) {
  LOG(5, "RET NC\n");
  if (lr35902->registers.f.c) {
    LOG(6, "not jumping\n");
    ++lr35902->registers.pc;
  } else {
    // pop 2B from stack
    lr35902->registers.sp += 2;
    // jump to that address
    LOG(6, "jumping\n");
    lr35902->registers.pc = rw(lr35902->mmu, lr35902->registers.sp);
  }
}

static void RST_28 (struct cpu* const lr35902) {
  LOG(5, "RST 0x28\n");
  // Push present address onto stack.
  lr35902->registers.sp -= 2;
  store_d16(lr35902, lr35902->registers.sp, lr35902->registers.pc);
  // Jump to address $0000 + n.
  lr35902->registers.pc = 0x28;
}

static void RLA (struct cpu* const lr35902) {
  LOG(5, "RLA\n");
  const uint8_t old_7_bit = (lr35902->registers.a & 0x80) >> 7;
  lr35902->registers.a <<= 1;
  lr35902->registers.a ^=
    (-lr35902->registers.f.c ^ lr35902->registers.a) & (1U << 0);
  lr35902->registers.f.z = !lr35902->registers.a;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 0;
  lr35902->registers.f.c = old_7_bit;
  ++lr35902->registers.pc;
}

static void RLCA (struct cpu* const lr35902) {
  LOG(5, "RLCA\n");
  const uint8_t old_7_bit = (lr35902->registers.a & 0x80) >> 7;
  lr35902->registers.a <<= 1;
  lr35902->registers.a ^=
    (-old_7_bit ^ lr35902->registers.a) & (1U << 0);
  lr35902->registers.f.z = !lr35902->registers.a;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 0;
  lr35902->registers.f.c = old_7_bit;
  ++lr35902->registers.pc;
}

static void RRA (struct cpu* const lr35902) {
  LOG(5, "RRA\n");
  const uint8_t old_0_bit = lr35902->registers.a & 0x01;
  lr35902->registers.a >>= 1;
  lr35902->registers.a ^=
    (-lr35902->registers.f.c ^ lr35902->registers.a) & (1U << 7);
  lr35902->registers.f.z = !lr35902->registers.a;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 0;
  lr35902->registers.f.c = old_0_bit;
  ++lr35902->registers.pc;
}

static void CP_d8 (struct cpu* const lr35902) {
  LOG(5, "CP d8\n");
  const uint8_t d8 = load_d8(lr35902);
  LOG(6, "where d8 == ");
  PBYTE(6, d8);
  const uint8_t result = lr35902->registers.a - d8;
  lr35902->registers.f.z = !result;
  lr35902->registers.f.n = 1;
  // not sure that these are right
  lr35902->registers.f.h = (result & 0x10) == 0x10;
  lr35902->registers.f.c = (result & 0x80) != 0;
  lr35902->registers.pc += 2;
}

static void CP_DEREF_HL (struct cpu* const lr35902) {
  LOG(5, "CP (HL)\n");
  const uint8_t subtrahend = rb(lr35902->mmu, lr35902->registers.hl);
  const uint8_t result = lr35902->registers.a - subtrahend;
  lr35902->registers.f.z = !result;
  lr35902->registers.f.n = 1;
  // not sure that these are right
  lr35902->registers.f.h = (result & 0x10) == 0x10;
  lr35902->registers.f.c = (result & 0x80) != 0;
  ++lr35902->registers.pc;
}

static void EI (struct cpu* const lr35902) {
  LOG(5, "EI\n");
  wb(lr35902->mmu, 0xFFFF, 0xFF); // ?
  lr35902->interrupts_enabled = 1;
  ++lr35902->registers.pc;
}

static void DI (struct cpu* const lr35902) {
  LOG(5, "DI\n");
  wb(lr35902->mmu, 0xFFFF, 0); // ?
  lr35902->interrupts_enabled = 0;
  ++lr35902->registers.pc;
}

#define OR_x(x, X) \
static void OR_ ## X (struct cpu* const lr35902) { \
  LOG(5, "OR " #X "\n"); \
  REG(a) |= x; \
  lr35902->registers.f.z = !lr35902->registers.a; \
  lr35902->registers.f.n = 0; \
  lr35902->registers.f.h = 0; \
  lr35902->registers.f.c = 0; \
  ++REG(pc); \
}

OR_x(REG(a), A);
OR_x(REG(b), B);
OR_x(REG(c), C);
OR_x(DEREF_READ(hl), DEREF_HL);

static void OR_d8 (struct cpu* const lr35902) {
  LOG(5, "OR d8\n");
  REG(a) |= load_d8(lr35902);
  lr35902->registers.f.z = !lr35902->registers.a;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 0;
  lr35902->registers.f.c = 0;
  ++REG(pc);
}

static void AND_C (struct cpu* const lr35902) {
  LOG(5, "AND C\n");
  lr35902->registers.a &= lr35902->registers.c;
  lr35902->registers.f.z = !lr35902->registers.a;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 1;
  lr35902->registers.f.c = 0;
  ++lr35902->registers.pc;
}

static void AND_d8 (struct cpu* const lr35902) {
  LOG(5, "AND d8\n");
  const uint8_t d8 = load_d8(lr35902);
  LOG(6, "where d8 == ");
  PBYTE(6, d8);
  lr35902->registers.a |= d8;
  lr35902->registers.f.z = !lr35902->registers.a;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 1;
  lr35902->registers.f.c = 0;
  lr35902->registers.pc += 2;
}

static void CPL (struct cpu* const lr35902) {
  LOG(5, "CPL\n");
  lr35902->registers.a = ~lr35902->registers.a;
  lr35902->registers.f.n = 1;
  lr35902->registers.f.h = 1;
  ++lr35902->registers.pc;
}

static const instr opcodes [256] = {
  NOP, LD_BC_d16, LD_DEREF_BC_A, INC_BC, INC_B, DEC_B, LD_B_d8, RLCA, 0, 0, 0, DEC_BC, INC_C, DEC_C, LD_C_d8, 0, // 0x
  0, LD_DE_d16, LD_DEREF_DE_A, INC_DE, INC_D, DEC_D, LD_D_d8, RLA, JR_r8, ADD_HL_DE, LD_A_DEREF_DE, 0, INC_E, DEC_E, LD_E_d8, RRA, // 1x
  JR_NZ_r8, LD_HL_d16, LD_DEREF_HL_INC_A, INC_HL, INC_H, 0, LD_H_d8, 0, JR_Z_r8, ADD_HL_HL, LD_A_DEREF_HL_INC, 0, INC_L, DEC_L, LD_L_d8, CPL, // 2x
  JR_NC_r8, LD_SP_d16, LD_DEREF_HL_DEC_A, 0, 0, DEC_DEREF_HL, LD_DEREF_HL_d8, 0, 0, 0, 0, 0, INC_A, DEC_A, LD_A_d8, 0, // 3x
  LD_B_B, 0, 0, 0, 0, 0, 0, LD_B_A, 0, 0, 0, 0, 0, 0, 0, LD_C_A, // 4x
  0, 0, 0, 0, 0, 0, LD_D_DEREF_HL, LD_D_A, 0, 0, 0, 0, 0, 0, LD_E_DEREF_HL, LD_E_A, // 5x
  LD_H_B, 0, 0, 0, 0, 0, LD_H_DEREF_HL, LD_H_A, 0, 0, 0, 0, 0, 0, LD_L_DEREF_HL, LD_L_A, // 6x
  0, 0, 0, 0, 0, 0, 0, LD_DEREF_HL_A, LD_A_B, LD_A_C, LD_A_D, LD_A_E, LD_A_H, LD_A_L, LD_A_DEREF_HL, 0, // 7x
  0, 0, 0, 0, 0, 0, ADD_A_DEREF_HL, ADD_A_A, 0, 0, 0, 0, 0, 0, 0, 0, // 8x
  SUB_B, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 9x
  0, AND_C, 0, 0, 0, 0, 0, 0, 0, XOR_C, 0, 0, 0, 0, 0, XOR_A, // Ax
  OR_B, OR_C, 0, 0, 0, 0, OR_DEREF_HL, OR_A, 0, 0, 0, 0, 0, 0, CP_DEREF_HL, 0, // Bx
  0, POP_BC, JP_NZ_a16, JP_a16, CALL_NZ_a16, PUSH_BC, ADD_A_d8, 0, RET_Z, RET, 0, handle_cb, 0, CALL_a16, ADC_A_d8, 0, // Cx
  RET_NC, POP_DE, 0, 0, 0, PUSH_DE, SUB_d8, 0, RET_C, 0, 0, 0, 0, 0, 0, 0, // Dx
  LDH_DEREF_a8_A, POP_HL, LD_DEREF_C_A, 0, 0, PUSH_HL, AND_d8, 0, 0, JP_DEREF_HL, LD_DEREF_a16_A, 0, 0, 0, 0, RST_28, // Ex
  LDH_A_DEREF_a8, POP_AF, 0, DI, 0, PUSH_AF, OR_d8, 0, LD_HL_SP_r8, LD_SP_HL, LD_A_DEREF_a16, EI, 0, 0, CP_d8, 0  // Fx
};

static instr decode (const struct cpu* const cpu) {
  uint16_t pc = cpu->registers.pc;
  uint8_t opcode = rb(cpu->mmu, pc);
  PBYTE(4, opcode);
  instr i = opcodes[opcode];
  // unknown instr
#ifndef NDEBUG
  if (i == 0) {
    fprintf(stderr, "Unknown opcode for byte " PRIbyte " @ " PRIshort "\n",
        opcode, pc);
  }
#endif
  assert(i != 0);
  return i;
}

int tick_once (struct cpu* const cpu) {
  instr i = decode(cpu);
  uint16_t pre_op_pc = cpu->registers.pc;
  i(cpu);
  assert(pre_op_pc != cpu->registers.pc);
  // TODO: make i() return int (cycles), return that here.
  return 4;
}

void init_cpu (struct cpu* const restrict cpu,
    struct mmu* const restrict mmu) {
  assert(cpu != NULL);
  assert(mmu != NULL);
  cpu->mmu = mmu;
  // Don't jump the pc forward if it looks like we might be running just the
  // BIOS.
  if (!mmu->has_bios && mmu->rom_size != 256) {
    // TODO: at the end of dmg bios, looks like af is 0x0101
    cpu->registers.af = 0x01B0;
    cpu->registers.bc = 0x0013;
    cpu->registers.de = 0x00D8;
    cpu->registers.hl = 0x014D;
    cpu->registers.sp = 0xFFFE;
    cpu->registers.pc = 0x0100;
  }
  cpu->interrupts_enabled = 1;
}
