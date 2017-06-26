#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "cpu.h"

void pbyte(const uint8_t b) {
  printf("0x%02hhX\n", b);
}

void pshort(const uint16_t s) {
  printf("0x%04hX\n", s);
}

static uint16_t load_16 (const size_t index, const uint8_t* const mem) {
  return (mem[index + 1] << 8) | mem[index];
}

static uint16_t load_d16 (const struct cpu* const lr35902) {
  uint16_t pc = lr35902->registers.pc;
  uint8_t* mem = lr35902->memory;

  return load_16(pc + 1, mem);
}

static uint8_t load_d8 (const struct cpu* const lr35902) {
  uint16_t pc = lr35902->registers.pc;
  uint8_t* mem = lr35902->memory;

  return mem[pc + 1];
}

static int8_t load_r8 (const struct cpu* const cpu) {
  // r8 are relative values and should be interpreted as signed.
  return (int8_t) load_d8(cpu);
}

static void store_d16 (const struct cpu* const cpu, const uint16_t addr,
    const uint16_t value) {
  uint8_t* mem = cpu->memory;
  mem[addr] = value & 0xFF;
  mem[addr + 1] = value >> 8;
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

static const instr cb_opcodes [256] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x
  0, CB_RL_C, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 1x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 2x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 3x
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
  uint8_t opcode = cpu->memory[pc];
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

static void INC_C (struct cpu* const lr35902) {
  puts("INC C");
  pbyte(lr35902->registers.c);
  ++lr35902->registers.c;
  pbyte(lr35902->registers.c);
  ++lr35902->registers.pc;
  lr35902->registers.f.z = lr35902->registers.c == 0;
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = (lr35902->registers.c & 0x10) == 0x10;
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

#define DEC_REGISTER(reg) do {\
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

// normal instructions (non-cb)
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

static void LD_C_A (struct cpu* const lr35902) {
  puts("LD C,A");
  lr35902->registers.c = lr35902->registers.a;
  ++lr35902->registers.pc;
}

static void LD_A_E (struct cpu* const lr35902) {
  puts("LD A,E");
  lr35902->registers.a = lr35902->registers.e;
  ++lr35902->registers.pc;
}

static void LD_HL_d16 (struct cpu* const lr35902) {
  puts("LD HL,d16");
  pshort(lr35902->registers.hl);
  lr35902->registers.hl = load_d16(lr35902);
  pshort(lr35902->registers.hl);
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
  uint8_t* mem = lr35902->memory;

  pbyte(mem[lr35902->registers.hl]);
  mem[lr35902->registers.hl] = lr35902->registers.a;
  pbyte(mem[lr35902->registers.hl]);

  pshort(lr35902->registers.hl);
  --lr35902->registers.hl;
  pshort(lr35902->registers.hl);

  ++lr35902->registers.pc;
}

static void LD_DEREF_HL_INC_A (struct cpu* const lr35902) {
  puts("LD (HL+),A");
  uint8_t* const mem = lr35902->memory;

  pbyte(mem[lr35902->registers.hl]);
  mem[lr35902->registers.hl] = lr35902->registers.a;
  pbyte(mem[lr35902->registers.hl]);

  pshort(lr35902->registers.hl);
  ++lr35902->registers.hl;
  pshort(lr35902->registers.hl);

  ++lr35902->registers.pc;
}

// TODO: combine this with LD_DEREF_HL_DEC_A
static void LD_DEREF_HL_A (struct cpu* const lr35902) {
  puts("LD (HL),A");
  uint8_t* mem = lr35902->memory;

  pbyte(mem[lr35902->registers.hl]);
  mem[lr35902->registers.hl] = lr35902->registers.a;
  pbyte(mem[lr35902->registers.hl]);

  ++lr35902->registers.pc;
}

static void LD_DEREF_C_A (struct cpu* const lr35902) {
  puts("LD (C),A");
  uint8_t* mem = lr35902->memory;

  // http://www.chrisantonellis.com/files/gameboy/gb-instructions.txt
  pbyte(mem[lr35902->registers.c + 0xFF00]);
  mem[lr35902->registers.c + 0xFF00] = lr35902->registers.a;
  pbyte(mem[lr35902->registers.c + 0xFF00]);

  // Errata in:
  // http://pastraiser.com/cpu/gameboy/gameboy_opcodes.html
  // instr is actually length 1, not 2
  ++lr35902->registers.pc;
}

static void LDH_DEREF_a8_A (struct cpu* const lr35902) {
  puts("LDH (a8),A");
  uint8_t* mem = lr35902->memory;
  uint8_t a8 = load_d8(lr35902);
  printf("where a8 == %d\n", a8);

  mem[a8 + 0xFF00] = lr35902->registers.a;

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

static void LD_L_d8 (struct cpu* const lr35902) {
  puts("LD L,d8");
  pbyte(lr35902->registers.l);
  lr35902->registers.l = load_d8(lr35902);
  pbyte(lr35902->registers.l);
  lr35902->registers.pc += 2;
}

static void LD_A_DEREF_DE (struct cpu* const lr35902) {
  puts("LD A,(DE)");
  uint8_t* const mem = lr35902->memory;

  pbyte(lr35902->registers.a);
  lr35902->registers.a = mem[lr35902->registers.de];
  pbyte(lr35902->registers.a);
  ++lr35902->registers.pc;
}

static void LD_DEREF_a16_A (struct cpu* const lr35902) {
  puts("LD (a16),A");
  const uint16_t a16 = load_d16(lr35902);
  pshort(a16);
  uint8_t* const mem = lr35902->memory;
  mem[a16] = lr35902->registers.a;
  lr35902->registers.pc += 3;
}

static void PUSH_BC (struct cpu* const lr35902) {
  puts("PUSH BC");

  lr35902->registers.sp -= 2;
  pshort(lr35902->registers.bc);
  store_d16(lr35902, lr35902->registers.sp, lr35902->registers.bc);
  ++lr35902->registers.pc;
}

static void POP_BC (struct cpu* const lr35902) {
  puts("POP BC");
  const uint8_t* const mem = lr35902->memory;

  pshort(lr35902->registers.bc);
  lr35902->registers.bc = load_16(lr35902->registers.sp, mem);
  pshort(lr35902->registers.bc);
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
  pshort(lr35902->registers.pc);
}

static void JR_Z_r8 (struct cpu* const lr35902) {
  puts("JR Z,r8");
  int8_t r8 = load_r8(lr35902);
  printf("where r8 == %d\n", r8);
  pshort(lr35902->registers.pc);

  __JR_COND_r8(lr35902, lr35902->registers.f.z);
  pshort(lr35902->registers.pc);
}

static void CALL_a16 (struct cpu* const lr35902) {
  puts("CALL a16");
  // push address of next instruction onto stack
  pshort(lr35902->registers.sp);
  lr35902->registers.sp -= 2;
  store_d16(lr35902, lr35902->registers.sp, lr35902->registers.pc + 3);

  // jump to address
  const uint8_t a16 = load_d16(lr35902);
  lr35902->registers.pc = a16;
}

static void RET (struct cpu* const lr35902) {
  puts("RET");
  uint8_t* const mem = lr35902->memory;
  // pop 2B from stack
  const uint16_t a = load_16(lr35902->registers.sp, mem);
  pshort(a);
  lr35902->registers.sp += 2;
  // jump to that address
  lr35902->registers.pc = a;
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

static const instr opcodes [256] = {
  0, 0, 0, 0, 0, DEC_B, LD_B_d8, 0, 0, 0, 0, 0, INC_C, DEC_C, LD_C_d8, 0, // 0x
  0, LD_DE_d16, 0, INC_DE, 0, 0, 0, RLA, JR_r8, 0, LD_A_DEREF_DE, 0, 0, 0, 0, 0, // 1x
  JR_NZ_r8, LD_HL_d16, LD_DEREF_HL_INC_A, INC_HL, 0, 0, 0, 0, JR_Z_r8, 0, 0, 0, 0, 0, LD_L_d8, 0, // 2x
  0, LD_SP_d16, LD_DEREF_HL_DEC_A, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, DEC_A, LD_A_d8, 0, // 3x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, LD_C_A, // 4x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 5x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 6x
  0, 0, 0, 0, 0, 0, 0, LD_DEREF_HL_A, 0, 0, 0, LD_A_E, 0, 0, 0, 0, // 7x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 8x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 9x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, XOR_A, // Ax
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Bx
  0, POP_BC, 0, 0, 0, PUSH_BC, 0, 0, 0, RET, 0, handle_cb, 0, CALL_a16, 0, 0, // Cx
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Dx
  LDH_DEREF_a8_A, 0, LD_DEREF_C_A, 0, 0, 0, 0, 0, 0, 0, LD_DEREF_a16_A, 0, 0, 0, 0, 0, // Ex
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, CP_d8, 0  // Fx
};

instr decode (const struct cpu* const cpu) {
  uint16_t pc = cpu->registers.pc;
  uint8_t opcode = cpu->memory[pc];
  pbyte(opcode);
  instr i = opcodes[opcode];
  // unknown instr
  assert(i != 0);
  return i;
}
