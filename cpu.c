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

static void CB_BIT_7_H (struct cpu* const lr35902) {
  puts("BIT 7,H");
  printf("h: %d\n", lr35902->registers.h);
  printf("z: %d\n", lr35902->registers.f.z);
  lr35902->registers.f.z = (lr35902->registers.h & (1 << 6)) != 0;
  printf("z: %d\n", lr35902->registers.f.z);
  lr35902->registers.f.n = 0;
  lr35902->registers.f.h = 0;
}

static const instr cb_opcodes [256] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 1x
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

static void LD_HL_d16 (struct cpu* const lr35902) {
  puts("LD HL,d16");
  pshort(lr35902->registers.hl);
  lr35902->registers.hl = load_d16(lr35902);
  pshort(lr35902->registers.hl);
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

static void LD_DEREF_C_A (struct cpu* const lr35902) {
  puts("LD (C),A)");
}

static void JR_NZ_r8 (struct cpu* const lr35902) {
  puts("JR NZ,r8");
  int8_t r8 = load_r8(lr35902);
  printf("where r8 == %d\n", r8);
  pshort(lr35902->registers.pc);

  if (lr35902->registers.f.z) {
    lr35902->registers.pc += 2;
    puts("not jumping");
  } else {
    puts("jumping");
    lr35902->registers.pc += r8 + 2;
  }
  pshort(lr35902->registers.pc);
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


static const instr opcodes [256] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, LD_C_d8, 0, // 0x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 1x
  JR_NZ_r8, LD_HL_d16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 2x
  0, LD_SP_d16, LD_DEREF_HL_DEC_A, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, LD_A_d8, 0, // 3x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 4x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 5x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 6x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 7x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 8x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 9x
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, XOR_A, // Ax
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Bx
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, handle_cb, 0, 0, 0, 0, // Cx
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Dx
  0, LD_DEREF_C_A, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // Ex
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  // Fx
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
