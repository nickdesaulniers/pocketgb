#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "cpu.h"

static void pbyte (const uint8_t b) {
  printf("0x%02hhX\n", b);
}

static void pshort (const uint16_t s) {
  printf("0x%04hX\n", s);
}

static void pflags (const struct flags f) {
  printf("Flags (Z N H C) (%u %u %u %u)\n", f.z, f.n, f.h, f.c);
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
  puts("BIT 7,H");
  printf("h: %d\n", lr35902->registers.h);
  printf("z: %d\n", lr35902->registers.f.z);
  lr35902->registers.f.z = (lr35902->registers.h & (1 << 6)) != 0;
  printf("z: %d\n", lr35902->registers.f.z);
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 0;
}

static void CB_RL_C (struct cpu* const lr35902) {
  // rotate left aka circular shift
  puts("RL C");
  const uint8_t n = lr35902->registers.c;
  const uint8_t r = (n << 1) | lr35902->registers.f.c;
  pbyte(lr35902->registers.c);
  lr35902->registers.f.z = r == 0;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 0;
  lr35902->registers.f.c = (n & 0x80) != 0;
  lr35902->registers.c = r;
  pbyte(lr35902->registers.c);
}

static void CB_SWAP (struct cpu* const lr35902) {
  puts("SWAP");
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
  pbyte(opcode);
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
  puts("INC B");
  pbyte(lr35902->registers.b);
  INC_REGISTER(b);
  pbyte(lr35902->registers.b);
}

static void INC_C (struct cpu* const lr35902) {
  puts("INC C");
  pbyte(lr35902->registers.c);
  INC_REGISTER(c);
  pbyte(lr35902->registers.c);
}

static void INC_H (struct cpu* const lr35902) {
  puts("INC H");
  pbyte(lr35902->registers.h);
  INC_REGISTER(h);
  pbyte(lr35902->registers.h);
}

static void INC_HL (struct cpu* const lr35902) {
  puts("INC HL");
  pshort(lr35902->registers.hl);
  ++lr35902->registers.hl;
  pshort(lr35902->registers.hl);
  ++lr35902->registers.pc;
}

static void INC_DE (struct cpu* const lr35902) {
  puts("INC DE");
  pshort(lr35902->registers.de);
  ++lr35902->registers.de;
  pshort(lr35902->registers.de);
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
  puts("DEC A");
  pbyte(lr35902->registers.a);
  DEC_REGISTER(a);
  pbyte(lr35902->registers.a);
}

static void DEC_B (struct cpu* const lr35902) {
  puts("DEC B");
  pbyte(lr35902->registers.b);
  DEC_REGISTER(b);
  pbyte(lr35902->registers.b);
}

static void DEC_C (struct cpu* const lr35902) {
  puts("DEC C");
  pbyte(lr35902->registers.c);
  DEC_REGISTER(c);
  pbyte(lr35902->registers.c);
}

static void DEC_D (struct cpu* const lr35902) {
  puts("DEC D");
  pbyte(lr35902->registers.d);
  DEC_REGISTER(d);
  pbyte(lr35902->registers.d);
}

static void DEC_E (struct cpu* const lr35902) {
  puts("DEC E");
  pbyte(lr35902->registers.e);
  DEC_REGISTER(e);
  pbyte(lr35902->registers.e);
}

static void DEC_BC (struct cpu* const lr35902) {
  puts("DEC BC");
  pshort(lr35902->registers.bc);
  DEC_REGISTER(bc);
  pshort(lr35902->registers.bc);
}

static void NOP (struct cpu* const lr35902) {
  puts("NOP");
  ++lr35902->registers.pc;
}

static void LD_SP_d16 (struct cpu* const lr35902) {
  puts("LD SP,d16");
  pshort(lr35902->registers.sp);
  lr35902->registers.sp = load_d16(lr35902);
  pshort(lr35902->registers.sp);
  lr35902->registers.pc += 3;
}

static void XOR_A (struct cpu* const lr35902) {
  puts("XOR A");
  lr35902->registers.a = 0;
  lr35902->registers.f.z = 1;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 0;
  lr35902->registers.f.c = 0;
  ++lr35902->registers.pc;
}

static void XOR_C (struct cpu* const lr35902) {
  puts("XOR C");
  lr35902->registers.a ^= lr35902->registers.c;
  lr35902->registers.f.z = 1;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 0;
  lr35902->registers.f.c = 0;
  ++lr35902->registers.pc;
}

static void ADD_A (struct cpu* const lr35902) {
  puts("ADD A");
  lr35902->registers.a += lr35902->registers.a;
  lr35902->registers.f.z = lr35902->registers.a == 0;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = (lr35902->registers.a & 0x10) == 0x10;
  lr35902->registers.f.c = (lr35902->registers.a & 0x80) != 0;
  ++lr35902->registers.pc;
}

static void ADD_DEREF_HL (struct cpu* const lr35902) {
  puts("ADD (HL)");
  lr35902->registers.a += rb(lr35902->mmu, lr35902->registers.hl);
  lr35902->registers.f.z = lr35902->registers.a == 0;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = (lr35902->registers.a & 0x10) == 0x10;
  lr35902->registers.f.c = (lr35902->registers.a & 0x80) != 0;
  ++lr35902->registers.pc;
}

static void ADD_HL_DE (struct cpu* const lr35902) {
  puts("ADD HL,DE");
  lr35902->registers.hl += lr35902->registers.de;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = (lr35902->registers.hl & 0x10) == 0x10;
  lr35902->registers.f.c = (lr35902->registers.hl & 0x80) != 0;
  ++lr35902->registers.pc;
}

static void SUB_B (struct cpu* const lr35902) {
  puts("SUB B");
  lr35902->registers.a -= lr35902->registers.b;
  lr35902->registers.f.z = lr35902->registers.a == 0;
  lr35902->registers.f.n = 1;
  lr35902->registers.f.h = (lr35902->registers.a & 0x10) == 0x10;
  lr35902->registers.f.c = (lr35902->registers.a & 0x80) != 0;
  ++lr35902->registers.pc;
}

static void LD_A_B (struct cpu* const lr35902) {
  puts("LD A,B");
  lr35902->registers.a = lr35902->registers.b;
  ++lr35902->registers.pc;
}

static void LD_A_C (struct cpu* const lr35902) {
  puts("LD A,C");
  lr35902->registers.a = lr35902->registers.c;
  ++lr35902->registers.pc;
}

static void LD_A_E (struct cpu* const lr35902) {
  puts("LD A,E");
  lr35902->registers.a = lr35902->registers.e;
  ++lr35902->registers.pc;
}

static void LD_A_H (struct cpu* const lr35902) {
  puts("LD A,H");
  lr35902->registers.a = lr35902->registers.h;
  ++lr35902->registers.pc;
}

static void LD_A_L (struct cpu* const lr35902) {
  puts("LD A,L");
  lr35902->registers.a = lr35902->registers.l;
  ++lr35902->registers.pc;
}

static void LD_B_A (struct cpu* const lr35902) {
  puts("LD B,A");
  lr35902->registers.b = lr35902->registers.a;
  ++lr35902->registers.pc;
}

static void LD_C_A (struct cpu* const lr35902) {
  puts("LD C,A");
  lr35902->registers.c = lr35902->registers.a;
  ++lr35902->registers.pc;
}

static void LD_D_A (struct cpu* const lr35902) {
  puts("LD D,A");
  lr35902->registers.d = lr35902->registers.a;
  ++lr35902->registers.pc;
}

static void LD_D_DEREF_HL (struct cpu* const lr35902) {
  puts("LD D,(HL)");
  lr35902->registers.d = rb(lr35902->mmu, lr35902->registers.hl);
  ++lr35902->registers.pc;
}

static void LD_E_A (struct cpu* const lr35902) {
  puts("LD E,A");
  lr35902->registers.e = lr35902->registers.a;
  ++lr35902->registers.pc;
}

static void LD_E_DEREF_HL (struct cpu* const lr35902) {
  puts("LD E,(HL)");
  lr35902->registers.e = rb(lr35902->mmu, lr35902->registers.hl);
  ++lr35902->registers.pc;
}

static void LD_H_A (struct cpu* const lr35902) {
  puts("LD H,A");
  lr35902->registers.h = lr35902->registers.a;
  ++lr35902->registers.pc;
}

static void LD_HL_d16 (struct cpu* const lr35902) {
  puts("LD HL,d16");
  pshort(lr35902->registers.hl);
  lr35902->registers.hl = load_d16(lr35902);
  pshort(lr35902->registers.hl);
  lr35902->registers.pc += 3;
}

static void LD_BC_d16 (struct cpu* const lr35902) {
  puts("LD BC,d16");
  pshort(lr35902->registers.bc);
  lr35902->registers.bc = load_d16(lr35902);
  pshort(lr35902->registers.bc);
  lr35902->registers.pc += 3;
}

static void LD_DE_d16 (struct cpu* const lr35902) {
  puts("LD DE,d16");
  pshort(lr35902->registers.de);
  lr35902->registers.de = load_d16(lr35902);
  pshort(lr35902->registers.de);
  lr35902->registers.pc += 3;
}

static void LD_DEREF_HL_DEC_A (struct cpu* const lr35902) {
  puts("LD (HL-),A");

  pbyte(rb(lr35902->mmu, lr35902->registers.hl));
  wb(lr35902->mmu, lr35902->registers.hl, lr35902->registers.a);
  pbyte(rb(lr35902->mmu, lr35902->registers.hl));

  pshort(lr35902->registers.hl);
  --lr35902->registers.hl;
  pshort(lr35902->registers.hl);

  ++lr35902->registers.pc;
}

static void LD_DEREF_HL_INC_A (struct cpu* const lr35902) {
  puts("LD (HL+),A");

  pbyte(rb(lr35902->mmu, lr35902->registers.hl));
  wb(lr35902->mmu, lr35902->registers.hl, lr35902->registers.a);
  pbyte(rb(lr35902->mmu, lr35902->registers.hl));

  pshort(lr35902->registers.hl);
  ++lr35902->registers.hl;
  pshort(lr35902->registers.hl);

  ++lr35902->registers.pc;
}

// TODO: combine this with LD_DEREF_HL_DEC_A
static void LD_DEREF_HL_A (struct cpu* const lr35902) {
  puts("LD (HL),A");

  pbyte(rb(lr35902->mmu, lr35902->registers.hl));
  wb(lr35902->mmu, lr35902->registers.hl, lr35902->registers.a);
  pbyte(rb(lr35902->mmu, lr35902->registers.hl));

  ++lr35902->registers.pc;
}

static void LD_DEREF_HL_d8 (struct cpu* const lr35902) {
  puts("LD (HL),d8");
  const uint8_t d8 = load_d8(lr35902);
  wb(lr35902->mmu, lr35902->registers.hl, d8);
  lr35902->registers.pc += 2;
}

static void LD_DEREF_C_A (struct cpu* const lr35902) {
  puts("LD (C),A");

  // http://www.chrisantonellis.com/files/gameboy/gb-instructions.txt
  pbyte(rb(lr35902->mmu, lr35902->registers.c + 0xFF00));
  wb(lr35902->mmu, lr35902->registers.c + 0xFF00, lr35902->registers.a);
  pbyte(rb(lr35902->mmu, lr35902->registers.c + 0xFF00));

  // Errata in:
  // http://pastraiser.com/cpu/gameboy/gameboy_opcodes.html
  // instr is actually length 1, not 2
  ++lr35902->registers.pc;
}

static void LDH_DEREF_a8_A (struct cpu* const lr35902) {
  puts("LDH (a8),A");
  uint8_t a8 = load_d8(lr35902);
  printf("where a8 == ");
  pbyte(a8);

  wb(lr35902->mmu, a8 + 0xFF00, lr35902->registers.a);

  lr35902->registers.pc += 2;
}

static void LDH_A_DEREF_a8 (struct cpu* const lr35902) {
  puts("LDH A,(a8)");
  uint8_t a8 = load_d8(lr35902);
  printf("where a8 == ");
  pbyte(a8);

  lr35902->registers.a = rb(lr35902->mmu, a8 + 0xFF00);
  lr35902->registers.pc += 2;
}

static void LD_C_d8 (struct cpu* const lr35902) {
  puts("LD C,d8");
  pbyte(lr35902->registers.c);
  lr35902->registers.c = load_d8(lr35902);
  pbyte(lr35902->registers.c);
  lr35902->registers.pc += 2;
}

static void LD_A_d8 (struct cpu* const lr35902) {
  puts("LD A,d8");
  pbyte(lr35902->registers.a);
  lr35902->registers.a = load_d8(lr35902);
  pbyte(lr35902->registers.a);
  lr35902->registers.pc += 2;
}

static void LD_B_d8 (struct cpu* const lr35902) {
  puts("LD B,d8");
  pbyte(lr35902->registers.b);
  lr35902->registers.b = load_d8(lr35902);
  pbyte(lr35902->registers.b);
  lr35902->registers.pc += 2;
}

static void LD_D_d8 (struct cpu* const lr35902) {
  puts("LD D,d8");
  pbyte(lr35902->registers.d);
  lr35902->registers.d = load_d8(lr35902);
  pbyte(lr35902->registers.d);
  lr35902->registers.pc += 2;
}

static void LD_L_d8 (struct cpu* const lr35902) {
  puts("LD L,d8");
  pbyte(lr35902->registers.l);
  lr35902->registers.l = load_d8(lr35902);
  pbyte(lr35902->registers.l);
  lr35902->registers.pc += 2;
}

static void LD_E_d8 (struct cpu* const lr35902) {
  puts("LD E,d8");
  pbyte(lr35902->registers.e);
  lr35902->registers.e = load_d8(lr35902);
  pbyte(lr35902->registers.e);
  lr35902->registers.pc += 2;
}

static void LD_A_DEREF_DE (struct cpu* const lr35902) {
  puts("LD A,(DE)");

  pbyte(lr35902->registers.a);
  lr35902->registers.a = rb(lr35902->mmu, lr35902->registers.de);
  pbyte(lr35902->registers.a);
  ++lr35902->registers.pc;
}

static void LD_A_DEREF_HL_INC (struct cpu* const lr35902) {
  puts("LD A,(HL+)");

  pbyte(lr35902->registers.a);
  lr35902->registers.a = rb(lr35902->mmu, lr35902->registers.hl);
  pbyte(lr35902->registers.a);

  pshort(lr35902->registers.hl);
  ++lr35902->registers.hl;
  pshort(lr35902->registers.hl);

  ++lr35902->registers.pc;
}

static void LD_DEREF_a16_A (struct cpu* const lr35902) {
  puts("LD (a16),A");
  const uint16_t a16 = load_d16(lr35902);
  pshort(a16);
  wb(lr35902->mmu, a16, lr35902->registers.a);
  lr35902->registers.pc += 3;
}

static void PUSH_BC (struct cpu* const lr35902) {
  puts("PUSH BC");

  lr35902->registers.sp -= 2;
  pshort(lr35902->registers.bc);
  store_d16(lr35902, lr35902->registers.sp, lr35902->registers.bc);
  ++lr35902->registers.pc;
}

static void PUSH_DE (struct cpu* const lr35902) {
  puts("PUSH DE");

  lr35902->registers.sp -= 2;
  pshort(lr35902->registers.de);
  store_d16(lr35902, lr35902->registers.sp, lr35902->registers.de);
  ++lr35902->registers.pc;
}

static void POP_BC (struct cpu* const lr35902) {
  puts("POP BC");

  pshort(lr35902->registers.bc);
  lr35902->registers.bc = rw(lr35902->mmu, lr35902->registers.sp);
  pshort(lr35902->registers.bc);
  lr35902->registers.sp += 2;
  ++lr35902->registers.pc;
}

static void POP_HL (struct cpu* const lr35902) {
  puts("POP HL");

  pshort(lr35902->registers.hl);
  lr35902->registers.hl = rw(lr35902->mmu, lr35902->registers.sp);
  pshort(lr35902->registers.hl);
  lr35902->registers.sp += 2;
  ++lr35902->registers.pc;
}

static void __JR_r8 (struct cpu* const lr35902) {
  lr35902->registers.pc += load_r8(lr35902) + 2;
}

static void JR_r8 (struct cpu* const lr35902) {
  puts("JR r8");
  // TODO: remove the double load when not debugging
  int8_t r8 = load_r8(lr35902);
  printf("where r8 == %d\n", r8);
  pshort(lr35902->registers.pc);
  __JR_r8(lr35902);
  pshort(lr35902->registers.pc);
}

static void __JR_COND_r8 (struct cpu* const lr35902, int cond) {
  if (cond) {
    puts("jumping");
    __JR_r8(lr35902);
  } else {
    puts("not jumping");
    lr35902->registers.pc += 2;
  }
}

static void JR_NZ_r8 (struct cpu* const lr35902) {
  puts("JR NZ,r8");
  int8_t r8 = load_r8(lr35902);
  printf("where r8 == %d\n", r8);
  pshort(lr35902->registers.pc);

  __JR_COND_r8(lr35902, !lr35902->registers.f.z);
}

static void JR_Z_r8 (struct cpu* const lr35902) {
  puts("JR Z,r8");
  int8_t r8 = load_r8(lr35902);
  printf("where r8 == %d\n", r8);
  pshort(lr35902->registers.pc);

  __JR_COND_r8(lr35902, lr35902->registers.f.z);
  pshort(lr35902->registers.pc);
}

static void JP_a16 (struct cpu* const lr35902) {
  puts("JP_a16");
  const uint16_t a16 = load_d16(lr35902);
  printf("jumping to ");
  pshort(a16);
  lr35902->registers.pc = a16;
}

static void JP_DEREF_HL (struct cpu* const lr35902) {
  puts("JP (HL)");
  lr35902->registers.pc = rw(lr35902->mmu, lr35902->registers.hl);
  printf("jumping to ");
  pshort(lr35902->registers.pc);
}

static void CALL_a16 (struct cpu* const lr35902) {
  puts("CALL a16");
  // push address of next instruction onto stack
  lr35902->registers.sp -= 2;
  store_d16(lr35902, lr35902->registers.sp, lr35902->registers.pc + 3);

  // jump to address
  const uint16_t a16 = load_d16(lr35902);
  lr35902->registers.pc = a16;
}

static void RET (struct cpu* const lr35902) {
  puts("RET");
  // pop 2B from stack
  const uint16_t a = rw(lr35902->mmu, lr35902->registers.sp);
  pshort(a);
  lr35902->registers.sp += 2;
  // jump to that address
  lr35902->registers.pc = a;
}

static void RST_28 (struct cpu* const lr35902) {
  puts("RST 0x28");
  // Push present address onto stack.
  lr35902->registers.sp -= 2;
  store_d16(lr35902, lr35902->registers.sp, lr35902->registers.pc);
  // Jump to address $0000 + n.
  lr35902->registers.pc = 0x28;
}

static void RLA (struct cpu* const lr35902) {
  puts("RLA");
  const uint8_t n = lr35902->registers.a;
  const uint8_t r = (n << 1) | lr35902->registers.f.c;
  pbyte(lr35902->registers.a);
  lr35902->registers.f.z = 0;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 0;
  lr35902->registers.f.c = (n & 0x80) != 0;
  lr35902->registers.a = r;
  pbyte(lr35902->registers.a);
  ++lr35902->registers.pc;
}

static void CP_d8 (struct cpu* const lr35902) {
  puts("CP d8");
  const uint8_t d8 = load_d8(lr35902);
  printf("where d8 == ");
  pbyte(d8);
  const uint8_t result = lr35902->registers.a - d8;
  lr35902->registers.f.z = !result;
  lr35902->registers.f.n = 1;
  // not sure that these are right
  lr35902->registers.f.h = (result & 0x10) == 0x10;
  lr35902->registers.f.c = (result & 0x80) != 0;
  lr35902->registers.pc += 2;
}

static void CP_DEREF_HL (struct cpu* const lr35902) {
  puts("CP (HL)");
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
  puts("EI");
  wb(lr35902->mmu, 0xFFFF, 0xFF); // ?
  lr35902->interrupts_enabled = 1;
  ++lr35902->registers.pc;
}

static void DI (struct cpu* const lr35902) {
  puts("DI");
  wb(lr35902->mmu, 0xFFFF, 0); // ?
  lr35902->interrupts_enabled = 0;
  ++lr35902->registers.pc;
}

static void OR_B (struct cpu* const lr35902) {
  puts("OR B");
  lr35902->registers.a |= lr35902->registers.b;
  lr35902->registers.f.z = !lr35902->registers.a;
  ++lr35902->registers.pc;
}

static void OR_C (struct cpu* const lr35902) {
  puts("OR C");
  lr35902->registers.a |= lr35902->registers.c;
  lr35902->registers.f.z = !lr35902->registers.a;
  ++lr35902->registers.pc;
}

static void AND_C (struct cpu* const lr35902) {
  puts("AND C");
  lr35902->registers.a &= lr35902->registers.c;
  lr35902->registers.f.z = !lr35902->registers.a;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 1;
  lr35902->registers.f.c = 0;
  ++lr35902->registers.pc;
}

static void AND_d8 (struct cpu* const lr35902) {
  puts("AND d8");
  const uint8_t d8 = load_d8(lr35902);
  printf("where d8 == ");
  pbyte(d8);
  lr35902->registers.a |= d8;
  lr35902->registers.f.z = !lr35902->registers.a;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 1;
  lr35902->registers.f.c = 0;
  lr35902->registers.pc += 2;
}

static void CPL (struct cpu* const lr35902) {
  puts("CPL");
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
  /*printf("PC@");*/
  /*pshort(pc);*/
  uint8_t opcode = rb(cpu->mmu, pc);
  pbyte(opcode);
  instr i = opcodes[opcode];
  // unknown instr
  assert(i != 0);
  return i;
}


void cpu_power_up (struct cpu* const cpu) {
  // TODO: are these just the state after the bios?
  // looks like yes, except for f. Likely a bug, or possible errata?
  pshort(cpu->registers.af);
  pflags(cpu->registers.f);
  /*pshort(cpu->registers.bc);*/
  /*pshort(cpu->registers.de);*/
  /*pshort(cpu->registers.hl);*/
  /*pshort(cpu->registers.sp);*/
  cpu->registers.af = 0x01B0;
  cpu->registers.bc = 0x0013;
  cpu->registers.de = 0x00D8;
  cpu->registers.hl = 0x014D;
  cpu->registers.sp = 0xFFFE;
  /*pshort(cpu->registers.pc);*/
  // TODO: likely set this if booted w/o bios
  /*cpu->registers.pc = 0x0100;*/
  cpu->interrupts_enabled = 1;
}
