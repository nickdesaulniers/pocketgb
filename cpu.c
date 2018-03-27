#include "cpu.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "logging.h"

#define REG(x) cpu->registers.x
#define CASE(op, block) \
  case op: \
    block; \
    break;

__attribute__((unused))
static void pflags (const int level, const struct flags f) {
  if (level <= LOG_LEVEL) {
      printf("Flags (Z N H C) (%u %u %u %u)\n", f.z, f.n, f.h, f.c);
    }
}

static void cb(struct cpu* const cpu);

static uint8_t deref_load(struct cpu* const cpu, const uint16_t addr) {
  cpu->tick_cycles += 4;
  return rb(cpu->mmu, addr);
}
static void deref_store(struct cpu* const cpu,
                        const uint16_t addr,
                        const uint8_t value) {
  cpu->tick_cycles += 4;
  wb(cpu->mmu, addr, value);
}
static void deref_store_word(struct cpu* const cpu, const uint16_t addr,
    const uint16_t value) {
  deref_store(cpu, addr, (uint8_t)value);
  deref_store(cpu, addr + 1, value >> 8);
}
// Fetches the next byte, incrementing the PC.
static uint8_t fetch_byte(struct cpu* const cpu) {
  return deref_load(cpu, REG(pc)++);
}
static uint16_t fetch_word(struct cpu* const cpu) {
  return (uint16_t)(((uint16_t)fetch_byte(cpu)) |
                    (((uint16_t)fetch_byte(cpu)) << 8));
}
static void push(struct cpu* const cpu, const uint16_t value) {
  LOG(6, "push " PRIshort " @ " PRIshort "\n", value, REG(sp));
  deref_store(cpu, --REG(sp), value >> 8);
  deref_store(cpu, --REG(sp), value & 0xFF);
}
static uint16_t pop(struct cpu* const cpu) {
  uint16_t value = deref_load(cpu, REG(sp)++);
  // Must be a second statement due to sequence points
  value |= deref_load(cpu, REG(sp)++) << 8;
  LOG(6, "pop " PRIshort " @ " PRIshort "\n", value, REG(sp));
  return value;
}
static void jump(struct cpu* const cpu, const uint16_t addr) {
  REG(pc) = addr;
  LOG(6, "jumping to " PRIshort "\n", REG(pc));
}
static void conditional_jump(struct cpu* const cpu, const uint16_t addr,
    const uint8_t cond) {
  assert(cond == 0 || cond == 1);
  if (cond) {
    cpu->tick_cycles += 4;
    jump(cpu, addr);
  } else {
    LOG(6, "not jumping\n");
  }
}
static void conditional_jump_relative(struct cpu* const cpu,
    const uint8_t cond) {
  assert(cond == 0 || cond == 1);
  const int8_t r8 = (int8_t)fetch_byte(cpu);
  assert(!((r8 == -2) && cond)); // inf loop
  const uint16_t addr = (uint16_t)((int16_t)REG(pc) + r8);
  conditional_jump(cpu, addr, cond);
}
static void call(struct cpu* const cpu, const uint8_t cond) {
  assert(cond == 0 || cond == 1);
  // Looks like the cycle penalty is paid even if we don't take the branch
  const uint16_t addr = fetch_word(cpu);
  if (cond) {
    push(cpu, REG(pc));
    conditional_jump(cpu, addr, cond);
  }
}
static void ret(struct cpu* const cpu, const uint8_t cond) {
  assert(cond == 0 || cond == 1);
  if (cond) {
    cpu->tick_cycles += 4;
    conditional_jump(cpu, pop(cpu), cond);
  }
}
// TODO: combine with call
static void rst(struct cpu* const cpu, const uint16_t addr) {
  assert(addr == 0x00 || addr == 0x08 || addr == 0x10 || addr == 0x18 ||
         addr == 0x20 || addr == 0x28 || addr == 0x30 || addr ==  0x38);
  cpu->tick_cycles += 4;
  push(cpu, REG(pc));
  jump(cpu, (uint16_t)addr);
}

static uint8_t get_bit(const uint8_t src, const uint8_t index) {
  assert(index < 8);
  return (src >> index) & 1U;
}

// logical operations
static void and(struct cpu* const cpu, const uint8_t x) {
  REG(a) &= x;
  REG(f.z) = !REG(a);
  REG(f.n) = REG(f.c) = 0;
  REG(f.h) = 1; // this is weird
}
static void dec(struct cpu* const cpu, uint8_t* const x) {
  --*x;
  REG(f.z) = !*x;
  REG(f.n) = 1;
  REG(f.h) = (*x & 0x0F) == 0x0F;
}
static void dec16(struct cpu* const cpu, uint16_t* const x) {
  --*x;
  cpu->tick_cycles += 4;
}
static void inc(struct cpu* const cpu, uint8_t* const x) {
  ++*x;
  REG(f.z) = !*x;
  REG(f.n) = 0;
  REG(f.h) = (*x & 0x0F) == 0x00;
}
static void inc16(struct cpu* const cpu, uint16_t* const x) {
  ++*x;
  cpu->tick_cycles += 4;
}
static void xor(struct cpu* const cpu, const uint8_t x) {
  REG(a) ^= x;
  REG(f.z) = !REG(a);
  REG(f.n) = REG(f.h) = REG(f.c) = 0;
}
static void or(struct cpu* const cpu, const uint8_t x) {
  REG(a) |= x;
  REG(f.z) = !REG(a);
  REG(f.n) = REG(f.h) = REG(f.c) = 0;
}
static void bit(struct cpu* const cpu, const uint8_t x, const uint8_t index) {
  REG(f.z) = !get_bit(x, index);
  REG(f.n) = 0;
  REG(f.h) = 1;
}
static void rotate_left(struct cpu* const cpu, uint8_t* const x) {
  const uint8_t carry = get_bit(*x, 7);
  *x = (*x << 1) | REG(f.c);
  REG(f.z) = !*x;
  REG(f.n) = REG(f.h) = 0;
  REG(f.c) = carry;
}
static void rotate_left_c(struct cpu* const cpu, uint8_t* const x) {
  *x = (*x << 1) | (*x >> 7);
  REG(f.z) = !*x;
  REG(f.n) = REG(f.h) = 0;
  REG(f.c) = get_bit(*x, 0);
}
static void rotate_right(struct cpu* const cpu, uint8_t* const x) {
  const uint8_t carry = get_bit(*x, 0);
  *x = (REG(f.c) << 7) | (*x >> 1);
  REG(f.z) = !*x;
  REG(f.n) = REG(f.h) = 0;
  REG(f.c) = carry;
}
static void rotate_right_c(struct cpu* const cpu, uint8_t* const x) {
  *x = (*x << 7) | (*x >> 1);
  REG(f.z) = !*x;
  REG(f.n) = REG(f.h) = 0;
  REG(f.c) = get_bit(*x, 7);
}
static void shift_right_logical(struct cpu* const cpu, uint8_t* const x) {
  const uint8_t carry = get_bit(*x, 0);
  *x >>= 1;
  REG(f.z) = !*x;
  REG(f.n) = REG(f.h) = 0;
  REG(f.c) = carry;
}
static void shift_left_arithmetic(struct cpu* const cpu, uint8_t* const x) {
  const uint8_t carry = get_bit(*x, 7);
  *x <<= 1;
  REG(f.z) = !*x;
  REG(f.n) = REG(f.h) = 0;
  REG(f.c) = carry;
}
static void shift_right_arithmetic(struct cpu* const cpu, uint8_t* const x) {
  const uint8_t carry = get_bit(*x, 0);
  *x = ((int8_t)*x) >> 1;
  REG(f.z) = !*x;
  REG(f.n) = REG(f.h) = 0;
  REG(f.c) = carry;
}

// SUB/SBC should reassign result, CP should not
static uint8_t subtract(struct cpu* const cpu, const uint8_t x, const uint8_t carry) {
  assert(carry == 0 || carry == 1);
  const uint16_t diff = REG(a) - x - carry;
  const uint16_t half = (REG(a) & 0x0F) - (x & 0x0F) - carry;
  REG(f.z) = ((uint8_t)diff) == 0;
  REG(f.n) = 1;
  REG(f.c) = diff > 0xFF;
  REG(f.h) = half > 0x0F;
  return (uint8_t)diff;
}
static void swap(struct cpu* const cpu, uint8_t* const x) {
  *x = (*x << 4) | (*x >> 4);
  REG(f.z) = !*x;
  REG(f.n) = REG(f.c) = REG(f.h) = 0;
}
static void add(struct cpu* const cpu, const uint8_t x, const uint8_t carry) {
  assert(carry == 0 || carry == 1);
  const uint16_t sum = REG(a) + x + carry;
  const uint8_t y = (REG(a) & 0x0F) + (x & 0x0F) + carry;
  REG(a) = (uint8_t)sum;
  REG(f.z) = !REG(a);
  REG(f.n) = 0;
  REG(f.c) = sum > 0xFF;
  REG(f.h) = y > 0x0F;
}
static void add16(struct cpu* const cpu, uint16_t* const a, const uint16_t b) {
  cpu->tick_cycles += 4;
  const uint32_t x = *a + b;
  const uint32_t y = (*a & 0x0FFFU) + (b & 0x0FFFU);
  REG(f.n) = 0;
  REG(f.c) = x > 0xFFFFU;
  REG(f.h) = y > 0x0FFFU;
  *a = (uint16_t)x;
}

uint8_t tick_once(struct cpu* const cpu) {
#ifndef NDEBUG
  const uint16_t pre_op_pc = REG(pc);
#endif
  cpu->tick_cycles = 0;
  const uint8_t op = fetch_byte(cpu);

  LOG(5, "== " PRIbyte " @ " PRIshort "\n", op, (uint16_t)(REG(pc) - 1));

  // TODO: check for interrupts
  switch (op) {
    CASE(0x00, {}) // NOP
    CASE(0x01, { REG(bc) = fetch_word(cpu); }) // LD BC,d16
    CASE(0x02, { deref_store(cpu, REG(bc), REG(a)); }) // LD (BC),A
    CASE(0x2F, { REG(a) = ~REG(a); REG(f.n) = REG(f.h) = 1; }) // CPL
    CASE(0x03, { inc16(cpu, &REG(bc)); }) // INC BC
    CASE(0x04, { inc(cpu, &REG(b)); }) // INC B
    CASE(0x05, { dec(cpu, &REG(b)); }) // DEC B
    CASE(0x06, { REG(b) = fetch_byte(cpu); }) // LD B,d8
    CASE(0x07, { rotate_left_c(cpu, &REG(a)); REG(f.z) = 0; }) // RLCA
    CASE(0x08, { deref_store_word(cpu, fetch_word(cpu), REG(sp)); }) // LD (a16),SP
    CASE(0x09, { add16(cpu, &REG(hl), REG(bc)); }) // ADD HL,BC
    CASE(0x0A, { REG(a) = deref_load(cpu, REG(bc)); }) // LD A,(BC)
    CASE(0x0B, { dec16(cpu, &REG(bc)); }) // DEC BC
    CASE(0x0C, { inc(cpu, &REG(c)); }) // INC C
    CASE(0x0D, { dec(cpu, &REG(c)); }) // DEC C
    CASE(0x0E, { REG(c) = fetch_byte(cpu); }) // LD C,d8
    CASE(0x0F, { rotate_right_c(cpu, &REG(a)); REG(f.z) = 0; }) // RRCA
    CASE(0x11, { REG(de) = fetch_word(cpu); }) // LD DE,d16
    CASE(0x12, { deref_store(cpu, REG(de), REG(a)); }) // LD (DE),A
    CASE(0x13, { inc16(cpu, &REG(de)); }) // INC DE
    CASE(0x14, { inc(cpu, &REG(d)); }) // INC D
    CASE(0x15, { dec(cpu, &REG(d)); }) // DEC D
    CASE(0x16, { REG(d) = fetch_byte(cpu); }) // LD D,d8
    CASE(0x17, { rotate_left(cpu, &REG(a)); REG(f.z) = 0; }) // RLA
    CASE(0x18, { conditional_jump_relative(cpu, 1); }) // JR r8
    CASE(0x19, { add16(cpu, &REG(hl), REG(de)); }) // ADD HL,DE
    CASE(0x1A, { REG(a) = deref_load(cpu, REG(de)); }) // LD A,(DE)
    CASE(0x1B, { dec16(cpu, &REG(de)); }) // DEC DE
    CASE(0x1C, { inc(cpu, &REG(e)); }) // INC E
    CASE(0x1D, { dec(cpu, &REG(e)); }) // DEC E
    CASE(0x1E, { REG(e) = fetch_byte(cpu); }) // LD E,d8
    CASE(0x1F, { rotate_right(cpu, &REG(a)); REG(f.z) = 0; }) // RRA
    CASE(0x20, { conditional_jump_relative(cpu, !REG(f.z)); }) // JR NZ,r8
    CASE(0x21, { REG(hl) = fetch_word(cpu); }) // LD HL,d16
    CASE(0x22, { deref_store(cpu, REG(hl)++, REG(a)); }) // LD (HL+),A
    CASE(0x23, { inc16(cpu, &REG(hl)); }) // INC HL
    CASE(0x24, { inc(cpu, &REG(h)); }) // INC H
    CASE(0x25, { dec(cpu, &REG(h)); }) // DEC H
    CASE(0x26, { REG(h) = fetch_byte(cpu); }) // LD H,d8
    CASE(0x28, { conditional_jump_relative(cpu, REG(f.z)); }) // JR Z,r8
    CASE(0x29, { add16(cpu, &REG(hl), REG(hl)); }) // ADD HL,HL
    CASE(0x2A, { REG(a) = deref_load(cpu, REG(hl)++); }) // LD A,(HL+)
    CASE(0x2B, { dec16(cpu, &REG(hl)); }) // DEC HL
    CASE(0x2C, { inc(cpu, &REG(l)); }) // INC L
    CASE(0x2D, { dec(cpu, &REG(l)); }) // DEC L
    CASE(0x2E, { REG(l) = fetch_byte(cpu); }) // LD L,d8
    CASE(0x30, { conditional_jump_relative(cpu, !REG(f.c)); }) // JR NC,r8
    CASE(0x31, { REG(sp) = fetch_word(cpu); }) // LD SP,d16
    CASE(0x32, { deref_store(cpu, REG(hl)--, REG(a)); }) // LD (HL-),A
    CASE(0x33, { inc16(cpu, &REG(sp)); }) // INC SP
    CASE(0x35, {
      uint8_t x = deref_load(cpu, REG(hl));
      dec(cpu, &x);
      deref_store(cpu, REG(hl), x);
    }) // DEC (HL)
    CASE(0x36, { deref_store(cpu, REG(hl), fetch_byte(cpu)); }) // LD (HL),d8
    CASE(0x37, { REG(f.c) = 1; REG(f.n) = REG(f.h) = 0; }) // SCF
    CASE(0x38, { conditional_jump_relative(cpu, REG(f.c)); }) // JR C,r8
    CASE(0x39, { add16(cpu, &REG(hl), REG(sp)); }) // ADD HL,SP
    CASE(0x3A, { REG(a) = deref_load(cpu, REG(hl)--); }) // LD A,(HL-)
    CASE(0x3B, { dec16(cpu, &REG(sp)); }) // DEC SP
    CASE(0x3C, { inc(cpu, &REG(a)); }) // INC A
    CASE(0x3D, { dec(cpu, &REG(a)); }) // DEC A
    CASE(0x3E, { REG(a) = fetch_byte(cpu); }) // LD A,d8
    CASE(0x3F, { REG(f.c) = !REG(f.c); REG(f.n) = REG(f.h) = 0; }) // CCF
    CASE(0x40, { REG(b) = REG(b); }) // LD B,B
    CASE(0x41, { REG(b) = REG(c); }) // LD B,C
    CASE(0x42, { REG(b) = REG(d); }) // LD B,D
    CASE(0x43, { REG(b) = REG(e); }) // LD B,E
    CASE(0x44, { REG(b) = REG(h); }) // LD B,H
    CASE(0x45, { REG(b) = REG(l); }) // LD B,L
    CASE(0x46, { REG(b) = deref_load(cpu, REG(hl)); }) // LD B,(HL)
    CASE(0x47, { REG(b) = REG(a); }) // LD B,A
    CASE(0x48, { REG(c) = REG(b); }) // LD C,B
    CASE(0x49, { REG(c) = REG(c); }) // LD C,C
    CASE(0x4A, { REG(c) = REG(d); }) // LD C,D
    CASE(0x4B, { REG(c) = REG(e); }) // LD C,E
    CASE(0x4C, { REG(c) = REG(h); }) // LD C,H
    CASE(0x4D, { REG(c) = REG(l); }) // LD C,L
    CASE(0x4E, { REG(c) = deref_load(cpu, REG(hl)); }) // LD C,(HL)
    CASE(0x4F, { REG(c) = REG(a); }) // LD C,A
    CASE(0x50, { REG(d) = REG(b); }) // LD D,B
    CASE(0x51, { REG(d) = REG(c); }) // LD D,C
    CASE(0x52, { REG(d) = REG(d); }) // LD D,D
    CASE(0x53, { REG(d) = REG(e); }) // LD D,E
    CASE(0x54, { REG(d) = REG(h); }) // LD D,H
    CASE(0x55, { REG(d) = REG(l); }) // LD D,L
    CASE(0x56, { REG(d) = deref_load(cpu, REG(hl)); }) // LD D,(HL)
    CASE(0x57, { REG(d) = REG(a); }) // LD D,A
    CASE(0x58, { REG(e) = REG(b); }) // LD E,B
    CASE(0x59, { REG(e) = REG(c); }) // LD E,C
    CASE(0x5A, { REG(e) = REG(d); }) // LD E,D
    CASE(0x5B, { REG(e) = REG(e); }) // LD E,E
    CASE(0x5C, { REG(e) = REG(h); }) // LD E,H
    CASE(0x5D, { REG(e) = REG(l); }) // LD E,L
    CASE(0x5E, { REG(e) = deref_load(cpu, REG(hl)); }) // LD E,(HL)
    CASE(0x5F, { REG(e) = REG(a); }) // LD E,A
    CASE(0x60, { REG(h) = REG(b); }) // LD H,B
    CASE(0x61, { REG(h) = REG(c); }) // LD H,C
    CASE(0x62, { REG(h) = REG(d); }) // LD H,D
    CASE(0x63, { REG(h) = REG(e); }) // LD H,E
    CASE(0x64, { REG(h) = REG(h); }) // LD H,H
    CASE(0x65, { REG(h) = REG(l); }) // LD H,L
    CASE(0x66, { REG(h) = deref_load(cpu, REG(hl)); }) // LD H,(HL)
    CASE(0x67, { REG(h) = REG(a); }) // LD H,A
    CASE(0x68, { REG(l) = REG(b); }) // LD L,B
    CASE(0x69, { REG(l) = REG(c); }) // LD L,C
    CASE(0x6A, { REG(l) = REG(d); }) // LD L,D
    CASE(0x6B, { REG(l) = REG(e); }) // LD L,E
    CASE(0x6C, { REG(l) = REG(h); }) // LD L,H
    CASE(0x6D, { REG(l) = REG(l); }) // LD L,L
    CASE(0x6E, { REG(l) = deref_load(cpu, REG(hl)); }) // LD L,(HL)
    CASE(0x6F, { REG(l) = REG(a); }) // LD L,A
    CASE(0x70, { deref_store(cpu, REG(hl), REG(b)); }) // LD (HL),B
    CASE(0x71, { deref_store(cpu, REG(hl), REG(c)); }) // LD (HL),C
    CASE(0x72, { deref_store(cpu, REG(hl), REG(d)); }) // LD (HL),D
    CASE(0x73, { deref_store(cpu, REG(hl), REG(e)); }) // LD (HL),E
    CASE(0x74, { deref_store(cpu, REG(hl), REG(h)); }) // LD (HL),H
    CASE(0x75, { deref_store(cpu, REG(hl), REG(l)); }) // LD (HL),L
    CASE(0x77, { deref_store(cpu, REG(hl), REG(a)); }) // LD (HL),A
    CASE(0x78, { REG(a) = REG(b); }) // LD A,B
    CASE(0x79, { REG(a) = REG(c); }) // LD A,C
    CASE(0x7A, { REG(a) = REG(d); }) // LD A,D
    CASE(0x7B, { REG(a) = REG(e); }) // LD A,E
    CASE(0x7C, { REG(a) = REG(h); }) // LD A,H
    CASE(0x7D, { REG(a) = REG(l); }) // LD A,L
    CASE(0x7E, { REG(a) = deref_load(cpu, REG(hl)); }) // LD A,(HL)
    CASE(0x7F, { REG(a) = REG(a); }) // LD A,A
    CASE(0x80, { add(cpu, REG(b), 0); }) // ADD B
    CASE(0x81, { add(cpu, REG(c), 0); }) // ADD C
    CASE(0x82, { add(cpu, REG(d), 0); }) // ADD D
    CASE(0x83, { add(cpu, REG(e), 0); }) // ADD E
    CASE(0x84, { add(cpu, REG(h), 0); }) // ADD H
    CASE(0x85, { add(cpu, REG(l), 0); }) // ADD L
    CASE(0x86, { add(cpu, deref_load(cpu, REG(hl)), 0); }) // ADD (HL)
    CASE(0x87, { add(cpu, REG(a), 0); }) // ADD A
    CASE(0x88, { add(cpu, REG(b), REG(f.c)); }) // ADC B
    CASE(0x89, { add(cpu, REG(c), REG(f.c)); }) // ADC C
    CASE(0x8A, { add(cpu, REG(d), REG(f.c)); }) // ADC D
    CASE(0x8B, { add(cpu, REG(e), REG(f.c)); }) // ADC E
    CASE(0x8C, { add(cpu, REG(h), REG(f.c)); }) // ADC H
    CASE(0x8D, { add(cpu, REG(l), REG(f.c)); }) // ADC L
    CASE(0x8E, { add(cpu, deref_load(cpu, REG(hl)), REG(f.c)); }) // ADC (HL)
    CASE(0x8F, { add(cpu, REG(a), REG(f.c)); }) // ADC A
    CASE(0x90, { REG(a) = subtract(cpu, REG(b), 0); }) // SUB B
    CASE(0x91, { REG(a) = subtract(cpu, REG(c), 0); }) // SUB C
    CASE(0x92, { REG(a) = subtract(cpu, REG(d), 0); }) // SUB D
    CASE(0x93, { REG(a) = subtract(cpu, REG(e), 0); }) // SUB E
    CASE(0x94, { REG(a) = subtract(cpu, REG(h), 0); }) // SUB H
    CASE(0x95, { REG(a) = subtract(cpu, REG(l), 0); }) // SUB L
    CASE(0x96, { REG(a) = subtract(cpu, deref_load(cpu, REG(hl)), 0); }) // SUB (HL)
    CASE(0x97, { REG(a) = subtract(cpu, REG(a), 0); }) // SUB A
    CASE(0x98, { REG(a) = subtract(cpu, REG(b), REG(f.c)); }) // SBC B
    CASE(0x99, { REG(a) = subtract(cpu, REG(c), REG(f.c)); }) // SBC C
    CASE(0x9A, { REG(a) = subtract(cpu, REG(d), REG(f.c)); }) // SBC D
    CASE(0x9B, { REG(a) = subtract(cpu, REG(e), REG(f.c)); }) // SBC E
    CASE(0x9C, { REG(a) = subtract(cpu, REG(h), REG(f.c)); }) // SBC H
    CASE(0x9D, { REG(a) = subtract(cpu, REG(l), REG(f.c)); }) // SBC L
    CASE(0x9E, { REG(a) = subtract(cpu, deref_load(cpu, REG(hl)), REG(f.c)); }) // SBC (HL)
    CASE(0x9F, { REG(a) = subtract(cpu, REG(a), REG(f.c)); }) // SBC A
    CASE(0xA0, { and(cpu, REG(b)); }) // AND B
    CASE(0xA1, { and(cpu, REG(c)); }) // AND C
    CASE(0xA2, { and(cpu, REG(d)); }) // AND D
    CASE(0xA3, { and(cpu, REG(e)); }) // AND E
    CASE(0xA4, { and(cpu, REG(h)); }) // AND H
    CASE(0xA5, { and(cpu, REG(l)); }) // AND L
    CASE(0xA6, { and(cpu, deref_load(cpu, REG(hl))); }) // AND (HL)
    CASE(0xA7, { and(cpu, REG(a)); }) // AND A
    CASE(0xA8, { xor(cpu, REG(b)); }) // XOR B
    CASE(0xA9, { xor(cpu, REG(c)); }) // XOR C
    CASE(0xAA, { xor(cpu, REG(d)); }) // XOR D
    CASE(0xAB, { xor(cpu, REG(e)); }) // XOR E
    CASE(0xAC, { xor(cpu, REG(h)); }) // XOR H
    CASE(0xAD, { xor(cpu, REG(l)); }) // XOR L
    CASE(0xAE, { xor(cpu, deref_load(cpu, REG(hl))); }) // XOR (HL)
    CASE(0xAF, { xor(cpu, REG(a)); }) // XOR A
    CASE(0xB0, { or(cpu, REG(b)); }) // OR B
    CASE(0xB1, { or(cpu, REG(c)); }) // OR C
    CASE(0xB2, { or(cpu, REG(d)); }) // OR D
    CASE(0xB3, { or(cpu, REG(e)); }) // OR E
    CASE(0xB4, { or(cpu, REG(h)); }) // OR H
    CASE(0xB5, { or(cpu, REG(l)); }) // OR L
    CASE(0xB6, { or(cpu, deref_load(cpu, REG(hl))); }) // OR (HL)
    CASE(0xB7, { or(cpu, REG(a)); }) // OR A
    CASE(0xB8, { subtract(cpu, REG(b), 0); }) // CP B
    CASE(0xB9, { subtract(cpu, REG(c), 0); }) // CP C
    CASE(0xBA, { subtract(cpu, REG(d), 0); }) // CP D
    CASE(0xBB, { subtract(cpu, REG(e), 0); }) // CP E
    CASE(0xBC, { subtract(cpu, REG(h), 0); }) // CP H
    CASE(0xBD, { subtract(cpu, REG(l), 0); }) // CP L
    CASE(0xBE, { subtract(cpu, deref_load(cpu, REG(hl)), 0); }) // CP (HL)
    CASE(0xBF, { subtract(cpu, REG(a), 0); }) // CP A
    CASE(0xC0, { ret(cpu, !REG(f.z)); }); // RET NZ
    CASE(0xC1, { REG(bc) = pop(cpu); }) // POP BC
    CASE(0xC2, { conditional_jump(cpu, fetch_word(cpu), !REG(f.z)); }) // JP NZ,a16
    CASE(0xC3, { jump(cpu, fetch_word(cpu)); }) // JP a16
    CASE(0xC4, { call(cpu, !REG(f.z)); }) // CALL NZ,a16
    CASE(0xC5, {
      push(cpu, REG(bc));
      // This is weird
      cpu->tick_cycles += 4;
    }) // PUSH BC
    CASE(0xC6, { add(cpu, fetch_byte(cpu), 0); }) // ADD d8
    CASE(0xC7, { rst(cpu, 0x00); }) // RST 0x00
    CASE(0xC8, { ret(cpu, REG(f.z)); }) // RET Z
    // TODO: this would be faster...
    /*CASE(0xC9, { jump(cpu, pop(cpu)); }) // RET*/
    CASE(0xC9, { ret(cpu, 1); }) // RET
    CASE(0xCA, { conditional_jump(cpu, fetch_word(cpu), REG(f.z)); }) // JP Z,a16
    CASE(0xCB, { cb(cpu); }) // CB prefix
    CASE(0xCC, { call(cpu, REG(f.z)); }) // CALL Z,a16
    CASE(0xCD, { call(cpu, 1); }) // CALL a16
    CASE(0xCE, { add(cpu, fetch_byte(cpu), REG(f.c)); }) // ADC d8
    CASE(0xCF, { rst(cpu, 0x08); }) // RST 0x00
    CASE(0xD0, { ret(cpu, !REG(f.c)); }) // RET NC
    CASE(0xD1, { REG(de) = pop(cpu); }) // POP DE
    CASE(0xD2, { conditional_jump(cpu, fetch_word(cpu), !REG(f.c)); }) // JP NC,a16
    CASE(0xD4, { call(cpu, !REG(f.c)); }) // CALL NC,a16
    CASE(0xD5, {
      push(cpu, REG(de));
      // This is weird
      cpu->tick_cycles += 4;
    }) // PUSH DE
    CASE(0xD6, { REG(a) = subtract(cpu, fetch_byte(cpu), 0); }) // SUB d8
    CASE(0xD7, { rst(cpu, 0x10); }) // RST 0x10
    CASE(0xD8, { ret(cpu, REG(f.c)); }) // RET C
    CASE(0xD9, {
      cpu->interrupts_enabled = 1;
      // TODO: this would be faster...
      /*jump(cpu, pop(cpu));*/
      ret(cpu, 1);
    }) // RETI
    CASE(0xDA, { conditional_jump(cpu, fetch_word(cpu), REG(f.c)); }) // JP C,a16
    CASE(0xDC, { call(cpu, REG(f.c)); }) // CALL C,a16
    CASE(0xDE, { REG(a) = subtract(cpu, fetch_byte(cpu), REG(f.c)); }) // SBC d8
    CASE(0xDF, { rst(cpu, 0x18); }) // RST 0x18
    CASE(0xE0, { deref_store(cpu, 0xFF00 | fetch_byte(cpu), REG(a)); }) // LDH (a8),A
    CASE(0xE1, { REG(hl) = pop(cpu); }) // POP HL
    CASE(0xE2, { deref_store(cpu, 0xFF00 | REG(c), REG(a)); }) // LD (C),A
    CASE(0xE5, {
      push(cpu, REG(hl));
      // This is weird
      cpu->tick_cycles += 4;
    }) // PUSH HL
    CASE(0xE6, { and(cpu, fetch_byte(cpu)); }) // AND d8
    CASE(0xE7, { rst(cpu, 0x20); }) // RST 0x20
    CASE(0xE9, { jump(cpu, REG(hl)); }) // JP HL
    CASE(0xEA, { deref_store(cpu, fetch_word(cpu), REG(a)); }) // LD (a16),A
    CASE(0xEE, { xor(cpu, fetch_byte(cpu)); }) // XOR d8
    CASE(0xEF, { rst(cpu, 0x28); }) // RST 0x28
    CASE(0xF0, {
      REG(a) = deref_load(cpu, 0xFF00 | fetch_byte(cpu));
    }) // LDH A,(a8)
    // Does not update padding!
    CASE(0xF1, { REG(af) = pop(cpu) & 0xFFF0; }) // POP AF
    CASE(0xF2, { REG(a) = deref_load(cpu, 0xFF00 | REG(c)); }) // LD A,(C)
    CASE(0xF3, { cpu->interrupts_enabled = 0; }) // DI
    CASE(0xF5, {
      push(cpu, REG(af));
      // This is weird
      cpu->tick_cycles += 4;
    }) // PUSH AF
    CASE(0xF6, { or(cpu, fetch_byte(cpu)); }) // OR d8
    CASE(0xF7, { rst(cpu, 0x30); }) // RST 0x30
    CASE(0xF9, {
      REG(sp) = REG(hl);
      cpu->tick_cycles += 4;
    }) // LD SP,HL
    CASE(0xFA, { REG(a) = deref_load(cpu, fetch_word(cpu)); }) // LD A,(a16)
    CASE(0xFB, { cpu->interrupts_enabled = 1; }) // EI
    CASE(0xFE, { subtract(cpu, fetch_byte(cpu), 0); }) // CP d8
    CASE(0xFF, { rst(cpu, 0x38); }) // RST 0x38
    default:
      fprintf(stderr, "Unhandled opcode: " PRIbyte "\n", op);
      exit(EXIT_FAILURE);
      break;
  }

  assert(cpu->tick_cycles >= 4);
  assert(cpu->tick_cycles <= 24);
  assert(pre_op_pc != REG(pc));  // Infinite loop detected
  return cpu->tick_cycles;
}

static void cb(struct cpu* const cpu) {
  cpu->tick_cycles += 4;
  const uint8_t op = fetch_byte(cpu);
  switch (op) {
    CASE(0x00, { rotate_left_c(cpu, &REG(b)); }) // RLC B
    CASE(0x01, { rotate_left_c(cpu, &REG(c)); }) // RLC C
    CASE(0x02, { rotate_left_c(cpu, &REG(d)); }) // RLC D
    CASE(0x03, { rotate_left_c(cpu, &REG(e)); }) // RLC E
    CASE(0x04, { rotate_left_c(cpu, &REG(h)); }) // RLC H
    CASE(0x05, { rotate_left_c(cpu, &REG(l)); }) // RLC L
    CASE(0x07, { rotate_left_c(cpu, &REG(a)); }) // RLC A
    CASE(0x08, { rotate_right_c(cpu, &REG(b)); }) // RLC B
    CASE(0x09, { rotate_right_c(cpu, &REG(c)); }) // RLC C
    CASE(0x0A, { rotate_right_c(cpu, &REG(d)); }) // RLC D
    CASE(0x0B, { rotate_right_c(cpu, &REG(e)); }) // RLC E
    CASE(0x0C, { rotate_right_c(cpu, &REG(h)); }) // RLC H
    CASE(0x0D, { rotate_right_c(cpu, &REG(l)); }) // RLC L
    CASE(0x0F, { rotate_right_c(cpu, &REG(a)); }) // RLC A
    CASE(0x10, { rotate_left(cpu, &REG(b)); }) // RL B
    CASE(0x11, { rotate_left(cpu, &REG(c)); }) // RL C
    CASE(0x12, { rotate_left(cpu, &REG(d)); }) // RL D
    CASE(0x13, { rotate_left(cpu, &REG(e)); }) // RL E
    CASE(0x14, { rotate_left(cpu, &REG(h)); }) // RL H
    CASE(0x15, { rotate_left(cpu, &REG(l)); }) // RL L
    CASE(0x17, { rotate_left(cpu, &REG(a)); }) // RL A
    CASE(0x18, { rotate_right(cpu, &REG(b)); }) // RR B
    CASE(0x19, { rotate_right(cpu, &REG(c)); }) // RR C
    CASE(0x1A, { rotate_right(cpu, &REG(d)); }) // RR D
    CASE(0x1B, { rotate_right(cpu, &REG(e)); }) // RR E
    CASE(0x1C, { rotate_right(cpu, &REG(h)); }) // RR H
    CASE(0x1D, { rotate_right(cpu, &REG(l)); }) // RR L
    CASE(0x1F, { rotate_right(cpu, &REG(a)); }) // RR A
    CASE(0x20, { shift_left_arithmetic(cpu, &REG(b)); }) // SLA B
    CASE(0x21, { shift_left_arithmetic(cpu, &REG(c)); }) // SLA C
    CASE(0x22, { shift_left_arithmetic(cpu, &REG(d)); }) // SLA D
    CASE(0x23, { shift_left_arithmetic(cpu, &REG(e)); }) // SLA E
    CASE(0x24, { shift_left_arithmetic(cpu, &REG(h)); }) // SLA H
    CASE(0x25, { shift_left_arithmetic(cpu, &REG(l)); }) // SLA L
    CASE(0x27, { shift_left_arithmetic(cpu, &REG(a)); }) // SLA A
    CASE(0x28, { shift_right_arithmetic(cpu, &REG(b)); }) // SRA B
    CASE(0x29, { shift_right_arithmetic(cpu, &REG(c)); }) // SRA C
    CASE(0x2A, { shift_right_arithmetic(cpu, &REG(d)); }) // SRA D
    CASE(0x2B, { shift_right_arithmetic(cpu, &REG(e)); }) // SRA E
    CASE(0x2C, { shift_right_arithmetic(cpu, &REG(h)); }) // SRA H
    CASE(0x2D, { shift_right_arithmetic(cpu, &REG(l)); }) // SRA L
    CASE(0x2F, { shift_right_arithmetic(cpu, &REG(a)); }) // SRA A
    CASE(0x30, { swap(cpu, &REG(b)); }) // SWAP B
    CASE(0x31, { swap(cpu, &REG(c)); }) // SWAP C
    CASE(0x32, { swap(cpu, &REG(d)); }) // SWAP D
    CASE(0x33, { swap(cpu, &REG(e)); }) // SWAP E
    CASE(0x34, { swap(cpu, &REG(h)); }) // SWAP H
    CASE(0x35, { swap(cpu, &REG(l)); }) // SWAP L
    CASE(0x37, { swap(cpu, &REG(a)); }) // SWAP A
    CASE(0x38, { shift_right_logical(cpu, &REG(b)); }) // SRL B
    CASE(0x39, { shift_right_logical(cpu, &REG(c)); }) // SRL C
    CASE(0x3A, { shift_right_logical(cpu, &REG(d)); }) // SRL D
    CASE(0x3B, { shift_right_logical(cpu, &REG(e)); }) // SRL E
    CASE(0x3C, { shift_right_logical(cpu, &REG(h)); }) // SRL H
    CASE(0x3D, { shift_right_logical(cpu, &REG(l)); }) // SRL L
    CASE(0x3F, { shift_right_logical(cpu, &REG(a)); }) // SRL A
    CASE(0x7C, { bit(cpu, REG(h), 7); }) // BIT 7,H
    default:
      fprintf(stderr, "Unhandled 0xCB opcode: " PRIbyte "\n", op);
      exit(EXIT_FAILURE);
      break;
  }
}

void init_cpu(struct cpu* const restrict cpu, struct mmu* const mmu) {
  assert(cpu != NULL);
  assert(mmu != NULL);
  cpu->mmu = mmu;
  // Don't jump the pc forward if it looks like we might be running just the
  // BIOS.
  if (!mmu->has_bios && mmu->rom_size != 256) {
    // TODO: is this the correct value of F at the end of BIOS?
    // TODO: might games depend on which specific bits are which flags?
    REG(af) = 0x010D;
    REG(bc) = 0x0013;
    REG(de) = 0x00D8;
    REG(hl) = 0x014D;
    REG(sp) = 0xFFFE;
    REG(pc) = 0x0100;
  }
  cpu->interrupts_enabled = 1;
}
