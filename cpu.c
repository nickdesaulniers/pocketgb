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
  const uint8_t n = lr35902->registers.c;
  const uint8_t r = (n << 1) | lr35902->registers.f.c;
  PBYTE(6, lr35902->registers.c);
  lr35902->registers.f.z = r == 0;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 0;
  lr35902->registers.f.c = (n & 0x80) != 0;
  lr35902->registers.c = r;
  PBYTE(6, lr35902->registers.c);
}

static void CB_SWAP (struct cpu* const lr35902) {
  LOG(5, "SWAP\n");
  const uint8_t x = lr35902->registers.a;
  lr35902->registers.a = (x & 0x0F) << 4 | (x & 0xF0) >> 4;
  lr35902->registers.f.z = !lr35902->registers.a;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 0;
  lr35902->registers.f.c = 0;
}

static const instr cb_opcodes [256] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x
  0, CB_RL_C, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 1x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 2x
  0, 0, 0, 0, 0, 0, 0, CB_SWAP, 0, 0, 0, 0, 0, 0, 0, 0, // 3x
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
#define INC_REGISTER(reg) do { \
  ++lr35902->registers.reg; \
  ++lr35902->registers.pc; \
  lr35902->registers.f.z = lr35902->registers.reg == 0; \
  lr35902->registers.f.n = 0; \
  lr35902->registers.f.h = (lr35902->registers.reg & 0x10) == 0x10; \
} while(0)

static void INC_B (struct cpu* const lr35902) {
  LOG(5, "INC B\n");
  PBYTE(6, lr35902->registers.b);
  INC_REGISTER(b);
  PBYTE(6, lr35902->registers.b);
}

static void INC_C (struct cpu* const lr35902) {
  LOG(5, "INC C\n");
  PBYTE(6, lr35902->registers.c);
  INC_REGISTER(c);
  PBYTE(6, lr35902->registers.c);
}

static void INC_H (struct cpu* const lr35902) {
  LOG(5, "INC H\n");
  PBYTE(6, lr35902->registers.h);
  INC_REGISTER(h);
  PBYTE(6, lr35902->registers.h);
}

static void INC_HL (struct cpu* const lr35902) {
  LOG(5, "INC HL\n");
  PSHORT(6, lr35902->registers.hl);
  ++lr35902->registers.hl;
  PSHORT(6, lr35902->registers.hl);
  ++lr35902->registers.pc;
}

static void INC_DE (struct cpu* const lr35902) {
  LOG(5, "INC DE\n");
  PSHORT(6, lr35902->registers.de);
  ++lr35902->registers.de;
  PSHORT(6, lr35902->registers.de);
  ++lr35902->registers.pc;
}

#define DEC_REGISTER(reg) do { \
  --lr35902->registers.reg; \
  ++lr35902->registers.pc; \
  lr35902->registers.f.z = lr35902->registers.reg == 0; \
  lr35902->registers.f.n = 1; \
  lr35902->registers.f.h = (lr35902->registers.reg & 0x10) == 0x10; \
} while(0)

static void DEC_A (struct cpu* const lr35902) {
  LOG(5, "DEC A\n");
  PBYTE(6, lr35902->registers.a);
  DEC_REGISTER(a);
  PBYTE(6, lr35902->registers.a);
}

static void DEC_B (struct cpu* const lr35902) {
  LOG(5, "DEC B\n");
  PBYTE(6, lr35902->registers.b);
  DEC_REGISTER(b);
  PBYTE(6, lr35902->registers.b);
}

static void DEC_C (struct cpu* const lr35902) {
  LOG(5, "DEC C\n");
  PBYTE(6, lr35902->registers.c);
  DEC_REGISTER(c);
  PBYTE(6, lr35902->registers.c);
}

static void DEC_D (struct cpu* const lr35902) {
  LOG(5, "DEC D\n");
  PBYTE(6, lr35902->registers.d);
  DEC_REGISTER(d);
  PBYTE(6, lr35902->registers.d);
}

static void DEC_E (struct cpu* const lr35902) {
  LOG(5, "DEC E\n");
  PBYTE(6, lr35902->registers.e);
  DEC_REGISTER(e);
  PBYTE(6, lr35902->registers.e);
}

static void DEC_BC (struct cpu* const lr35902) {
  LOG(5, "DEC BC\n");
  PSHORT(6, lr35902->registers.bc);
  DEC_REGISTER(bc);
  PSHORT(6, lr35902->registers.bc);
}

static void NOP (struct cpu* const lr35902) {
  LOG(5, "NOP\n");
  ++lr35902->registers.pc;
}

static void LD_SP_d16 (struct cpu* const lr35902) {
  LOG(5, "LD SP,d16\n");
  PSHORT(6, lr35902->registers.sp);
  lr35902->registers.sp = load_d16(lr35902);
  PSHORT(6, lr35902->registers.sp);
  lr35902->registers.pc += 3;
}

static void XOR_A (struct cpu* const lr35902) {
  LOG(5, "XOR A\n");
  lr35902->registers.a = 0;
  lr35902->registers.f.z = 1;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 0;
  lr35902->registers.f.c = 0;
  ++lr35902->registers.pc;
}

static void XOR_C (struct cpu* const lr35902) {
  LOG(5, "XOR C\n");
  lr35902->registers.a ^= lr35902->registers.c;
  lr35902->registers.f.z = 1;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 0;
  lr35902->registers.f.c = 0;
  ++lr35902->registers.pc;
}

static void ADD_A (struct cpu* const lr35902) {
  LOG(5, "ADD A\n");
  lr35902->registers.a += lr35902->registers.a;
  lr35902->registers.f.z = lr35902->registers.a == 0;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = (lr35902->registers.a & 0x10) == 0x10;
  lr35902->registers.f.c = (lr35902->registers.a & 0x80) != 0;
  ++lr35902->registers.pc;
}

static void ADD_DEREF_HL (struct cpu* const lr35902) {
  LOG(5, "ADD (HL)\n");
  lr35902->registers.a += rb(lr35902->mmu, lr35902->registers.hl);
  lr35902->registers.f.z = lr35902->registers.a == 0;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = (lr35902->registers.a & 0x10) == 0x10;
  lr35902->registers.f.c = (lr35902->registers.a & 0x80) != 0;
  ++lr35902->registers.pc;
}

static void ADD_HL_DE (struct cpu* const lr35902) {
  LOG(5, "ADD HL,DE\n");
  lr35902->registers.hl += lr35902->registers.de;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = (lr35902->registers.hl & 0x10) == 0x10;
  lr35902->registers.f.c = (lr35902->registers.hl & 0x80) != 0;
  ++lr35902->registers.pc;
}

static void SUB_B (struct cpu* const lr35902) {
  LOG(5, "SUB B\n");
  lr35902->registers.a -= lr35902->registers.b;
  lr35902->registers.f.z = lr35902->registers.a == 0;
  lr35902->registers.f.n = 1;
  lr35902->registers.f.h = (lr35902->registers.a & 0x10) == 0x10;
  lr35902->registers.f.c = (lr35902->registers.a & 0x80) != 0;
  ++lr35902->registers.pc;
}

static void LD_A_B (struct cpu* const lr35902) {
  LOG(5, "LD A,B\n");
  lr35902->registers.a = lr35902->registers.b;
  ++lr35902->registers.pc;
}

static void LD_A_C (struct cpu* const lr35902) {
  LOG(5, "LD A,C\n");
  lr35902->registers.a = lr35902->registers.c;
  ++lr35902->registers.pc;
}

static void LD_A_E (struct cpu* const lr35902) {
  LOG(5, "LD A,E\n");
  lr35902->registers.a = lr35902->registers.e;
  ++lr35902->registers.pc;
}

static void LD_A_H (struct cpu* const lr35902) {
  LOG(5, "LD A,H\n");
  lr35902->registers.a = lr35902->registers.h;
  ++lr35902->registers.pc;
}

static void LD_A_L (struct cpu* const lr35902) {
  LOG(5, "LD A,L\n");
  lr35902->registers.a = lr35902->registers.l;
  ++lr35902->registers.pc;
}

static void LD_B_A (struct cpu* const lr35902) {
  LOG(5, "LD B,A\n");
  lr35902->registers.b = lr35902->registers.a;
  ++lr35902->registers.pc;
}

static void LD_C_A (struct cpu* const lr35902) {
  LOG(5, "LD C,A\n");
  lr35902->registers.c = lr35902->registers.a;
  ++lr35902->registers.pc;
}

static void LD_D_A (struct cpu* const lr35902) {
  LOG(5, "LD D,A\n");
  lr35902->registers.d = lr35902->registers.a;
  ++lr35902->registers.pc;
}

static void LD_D_DEREF_HL (struct cpu* const lr35902) {
  LOG(5, "LD D,(HL)\n");
  lr35902->registers.d = rb(lr35902->mmu, lr35902->registers.hl);
  ++lr35902->registers.pc;
}

static void LD_E_A (struct cpu* const lr35902) {
  LOG(5, "LD E,A\n");
  lr35902->registers.e = lr35902->registers.a;
  ++lr35902->registers.pc;
}

static void LD_E_DEREF_HL (struct cpu* const lr35902) {
  LOG(5, "LD E,(HL)\n");
  lr35902->registers.e = rb(lr35902->mmu, lr35902->registers.hl);
  ++lr35902->registers.pc;
}

static void LD_H_A (struct cpu* const lr35902) {
  LOG(5, "LD H,A\n");
  lr35902->registers.h = lr35902->registers.a;
  ++lr35902->registers.pc;
}

static void LD_HL_d16 (struct cpu* const lr35902) {
  LOG(5, "LD HL,d16\n");
  PSHORT(6, lr35902->registers.hl);
  lr35902->registers.hl = load_d16(lr35902);
  PSHORT(6, lr35902->registers.hl);
  lr35902->registers.pc += 3;
}

static void LD_BC_d16 (struct cpu* const lr35902) {
  LOG(5, "LD BC,d16\n");
  PSHORT(6, lr35902->registers.bc);
  lr35902->registers.bc = load_d16(lr35902);
  PSHORT(6, lr35902->registers.bc);
  lr35902->registers.pc += 3;
}

static void LD_DE_d16 (struct cpu* const lr35902) {
  LOG(5, "LD DE,d16\n");
  PSHORT(6, lr35902->registers.de);
  lr35902->registers.de = load_d16(lr35902);
  PSHORT(6, lr35902->registers.de);
  lr35902->registers.pc += 3;
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

// TODO: combine this with LD_DEREF_HL_DEC_A
static void LD_DEREF_HL_A (struct cpu* const lr35902) {
  LOG(5, "LD (HL),A\n");

  PBYTE(6, rb(lr35902->mmu, lr35902->registers.hl));
  wb(lr35902->mmu, lr35902->registers.hl, lr35902->registers.a);
  PBYTE(6, rb(lr35902->mmu, lr35902->registers.hl));

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

static void LD_C_d8 (struct cpu* const lr35902) {
  LOG(5, "LD C,d8\n");
  PBYTE(6, lr35902->registers.c);
  lr35902->registers.c = load_d8(lr35902);
  PBYTE(6, lr35902->registers.c);
  lr35902->registers.pc += 2;
}

static void LD_A_d8 (struct cpu* const lr35902) {
  LOG(5, "LD A,d8\n");
  PBYTE(6, lr35902->registers.a);
  lr35902->registers.a = load_d8(lr35902);
  PBYTE(6, lr35902->registers.a);
  lr35902->registers.pc += 2;
}

static void LD_B_d8 (struct cpu* const lr35902) {
  LOG(5, "LD B,d8\n");
  PBYTE(6, lr35902->registers.b);
  lr35902->registers.b = load_d8(lr35902);
  PBYTE(6, lr35902->registers.b);
  lr35902->registers.pc += 2;
}

static void LD_D_d8 (struct cpu* const lr35902) {
  LOG(5, "LD D,d8\n");
  PBYTE(6, lr35902->registers.d);
  lr35902->registers.d = load_d8(lr35902);
  PBYTE(6, lr35902->registers.d);
  lr35902->registers.pc += 2;
}

static void LD_L_d8 (struct cpu* const lr35902) {
  LOG(5, "LD L,d8\n");
  PBYTE(6, lr35902->registers.l);
  lr35902->registers.l = load_d8(lr35902);
  PBYTE(6, lr35902->registers.l);
  lr35902->registers.pc += 2;
}

static void LD_E_d8 (struct cpu* const lr35902) {
  LOG(5, "LD E,d8\n");
  PBYTE(6, lr35902->registers.e);
  lr35902->registers.e = load_d8(lr35902);
  PBYTE(6, lr35902->registers.e);
  lr35902->registers.pc += 2;
}

static void LD_A_DEREF_DE (struct cpu* const lr35902) {
  LOG(5, "LD A,(DE)\n");

  PBYTE(6, lr35902->registers.a);
  lr35902->registers.a = rb(lr35902->mmu, lr35902->registers.de);
  PBYTE(6, lr35902->registers.a);
  ++lr35902->registers.pc;
}

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

static void LD_DEREF_a16_A (struct cpu* const lr35902) {
  LOG(5, "LD (a16),A\n");
  const uint16_t a16 = load_d16(lr35902);
  PSHORT(6, a16);
  wb(lr35902->mmu, a16, lr35902->registers.a);
  lr35902->registers.pc += 3;
}

static void PUSH_BC (struct cpu* const lr35902) {
  LOG(5, "PUSH BC\n");

  lr35902->registers.sp -= 2;
  PSHORT(6, lr35902->registers.bc);
  store_d16(lr35902, lr35902->registers.sp, lr35902->registers.bc);
  ++lr35902->registers.pc;
}

static void PUSH_DE (struct cpu* const lr35902) {
  LOG(5, "PUSH DE\n");

  lr35902->registers.sp -= 2;
  PSHORT(6, lr35902->registers.de);
  store_d16(lr35902, lr35902->registers.sp, lr35902->registers.de);
  ++lr35902->registers.pc;
}

static void POP_BC (struct cpu* const lr35902) {
  LOG(5, "POP BC\n");

  PSHORT(6, lr35902->registers.bc);
  lr35902->registers.bc = rw(lr35902->mmu, lr35902->registers.sp);
  PSHORT(6, lr35902->registers.bc);
  lr35902->registers.sp += 2;
  ++lr35902->registers.pc;
}

static void POP_HL (struct cpu* const lr35902) {
  LOG(5, "POP HL\n");

  PSHORT(6, lr35902->registers.hl);
  lr35902->registers.hl = rw(lr35902->mmu, lr35902->registers.sp);
  PSHORT(6, lr35902->registers.hl);
  lr35902->registers.sp += 2;
  ++lr35902->registers.pc;
}

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

static void JP_a16 (struct cpu* const lr35902) {
  LOG(5, "JP_a16\n");
  const uint16_t a16 = load_d16(lr35902);
  LOG(6, "jumping to ");
  PSHORT(6, a16);
  lr35902->registers.pc = a16;
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

static void RET (struct cpu* const lr35902) {
  LOG(5, "RET\n");
  // pop 2B from stack
  const uint16_t a = rw(lr35902->mmu, lr35902->registers.sp);
  PSHORT(6, a);
  lr35902->registers.sp += 2;
  // jump to that address
  lr35902->registers.pc = a;
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
  const uint8_t n = lr35902->registers.a;
  const uint8_t r = (n << 1) | lr35902->registers.f.c;
  PBYTE(6, lr35902->registers.a);
  lr35902->registers.f.z = 0;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 0;
  lr35902->registers.f.c = (n & 0x80) != 0;
  lr35902->registers.a = r;
  PBYTE(6, lr35902->registers.a);
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

static void OR_B (struct cpu* const lr35902) {
  LOG(5, "OR B\n");
  lr35902->registers.a |= lr35902->registers.b;
  lr35902->registers.f.z = !lr35902->registers.a;
  ++lr35902->registers.pc;
}

static void OR_C (struct cpu* const lr35902) {
  LOG(5, "OR C\n");
  lr35902->registers.a |= lr35902->registers.c;
  lr35902->registers.f.z = !lr35902->registers.a;
  ++lr35902->registers.pc;
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
  NOP, LD_BC_d16, 0, 0, INC_B, DEC_B, LD_B_d8, 0, 0, 0, 0, DEC_BC, INC_C, DEC_C, LD_C_d8, 0, // 0x
  0, LD_DE_d16, 0, INC_DE, 0, DEC_D, LD_D_d8, RLA, JR_r8, ADD_HL_DE, LD_A_DEREF_DE, 0, 0, DEC_E, LD_E_d8, 0, // 1x
  JR_NZ_r8, LD_HL_d16, LD_DEREF_HL_INC_A, INC_HL, INC_H, 0, 0, 0, JR_Z_r8, 0, LD_A_DEREF_HL_INC, 0, 0, 0, LD_L_d8, CPL, // 2x
  0, LD_SP_d16, LD_DEREF_HL_DEC_A, 0, 0, 0, LD_DEREF_HL_d8, 0, 0, 0, 0, 0, 0, DEC_A, LD_A_d8, 0, // 3x
  0, 0, 0, 0, 0, 0, 0, LD_B_A, 0, 0, 0, 0, 0, 0, 0, LD_C_A, // 4x
  0, 0, 0, 0, 0, 0, LD_D_DEREF_HL, LD_D_A, 0, 0, 0, 0, 0, 0, LD_E_DEREF_HL, LD_E_A, // 5x
  0, 0, 0, 0, 0, 0, 0, LD_H_A, 0, 0, 0, 0, 0, 0, 0, 0, // 6x
  0, 0, 0, 0, 0, 0, 0, LD_DEREF_HL_A, LD_A_B, LD_A_C, 0, LD_A_E, LD_A_H, LD_A_L, 0, 0, // 7x
  0, 0, 0, 0, 0, 0, ADD_DEREF_HL, ADD_A, 0, 0, 0, 0, 0, 0, 0, 0, // 8x
  SUB_B, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 9x
  0, AND_C, 0, 0, 0, 0, 0, 0, 0, XOR_C, 0, 0, 0, 0, 0, XOR_A, // Ax
  OR_B, OR_C, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, CP_DEREF_HL, 0, // Bx
  0, POP_BC, 0, JP_a16, 0, PUSH_BC, 0, 0, 0, RET, 0, handle_cb, 0, CALL_a16, 0, 0, // Cx
  0, 0, 0, 0, 0, PUSH_DE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Dx
  LDH_DEREF_a8_A, POP_HL, LD_DEREF_C_A, 0, 0, 0, AND_d8, 0, 0, JP_DEREF_HL, LD_DEREF_a16_A, 0, 0, 0, 0, RST_28, // Ex
  LDH_A_DEREF_a8, 0, 0, DI, 0, 0, 0, 0, 0, 0, 0, EI, 0, 0, CP_d8, 0  // Fx
};

instr decode (const struct cpu* const cpu) {
  uint16_t pc = cpu->registers.pc;
  uint8_t opcode = rb(cpu->mmu, pc);
  PBYTE(4, opcode);
  instr i = opcodes[opcode];
  // unknown instr
  assert(i != 0);
  return i;
}


void cpu_power_up (struct cpu* const cpu) {
  // TODO: are these just the state after the bios?
  // looks like yes, except for f. Likely a bug, or possible errata?
  PSHORT(6, cpu->registers.af);
  pflags(6, cpu->registers.f);
  /*PSHORT(6, cpu->registers.bc);*/
  /*PSHORT(6, cpu->registers.de);*/
  /*PSHORT(6, cpu->registers.hl);*/
  /*PSHORT(6, cpu->registers.sp);*/
  cpu->registers.af = 0x01B0;
  cpu->registers.bc = 0x0013;
  cpu->registers.de = 0x00D8;
  cpu->registers.hl = 0x014D;
  cpu->registers.sp = 0xFFFE;
  /*PSHORT(6, cpu->registers.pc);*/
  // TODO: likely set this if booted w/o bios
  /*cpu->registers.pc = 0x0100;*/
  cpu->interrupts_enabled = 1;
}
