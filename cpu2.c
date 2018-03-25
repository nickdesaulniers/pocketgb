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

// Fetches the next byte, incrementing the PC.
static uint8_t fetch_byte(struct cpu* const cpu) {
  cpu->tick_cycles += 4;
  return rb(cpu->mmu, REG(pc)++);
}
static uint16_t fetch_word(struct cpu* const cpu) {
  return (uint16_t)(((uint16_t)fetch_byte(cpu)) |
                    (((uint16_t)fetch_byte(cpu)) << 8));
}
static void jump(struct cpu* const cpu, const uint16_t addr) {
  cpu->tick_cycles += 4;
  REG(pc) = addr;
  LOG(6, "jumping to " PRIshort "\n", REG(pc));
}
static void jump_relative(struct cpu* const cpu) {
  // Its relative to the end of the JR inst. At this point, we've already
  // advanced the PC reading the opcode.
  jump(cpu, (uint16_t)(REG(pc) + (int8_t)(fetch_byte(cpu))) + 1U);
}
static void conditional_jump_relative(struct cpu* const cpu,
                                      const uint8_t cond) {
  if (cond) {
    jump_relative(cpu);
  } else {
    LOG(6, "not jumping\n");
    ++REG(pc);
  }
}

static void deref_store(struct cpu* const cpu,
                        const uint16_t addr,
                        const uint8_t value) {
  cpu->tick_cycles += 4;
  wb(cpu->mmu, addr, value);
}
static uint8_t deref_load(struct cpu* const cpu, const uint16_t addr) {
  cpu->tick_cycles += 4;
  return rb(cpu->mmu, addr);
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

static void call(struct cpu* const cpu, const uint8_t cond) {
  assert(cond == 0 || cond == 1);
  // Looks like the cycle penalty is paid even if we don't take the branch
  const uint16_t addr = fetch_word(cpu);
  if (cond) {
    push(cpu, REG(pc));
    jump(cpu, addr);
  }
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
static void inc(struct cpu* const cpu, uint8_t* const x) {
  ++*x;
  REG(f.z) = !*x;
  REG(f.n) = 0;
  REG(f.h) = (*x & 0x0F) == 0x00;
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
  // REG(f.z) is handled different when destination register is A
  REG(f.c) = carry;
  REG(f.h) = REG(f.n) = 0;
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

uint8_t tick_once(struct cpu* const cpu) {
#ifndef NDEBUG
  const uint16_t pre_op_pc = REG(pc);
#endif
  cpu->tick_cycles = 0;
  const uint8_t op = fetch_byte(cpu);

  LOG(5, "== " PRIbyte " @ " PRIshort "\n", op, REG(pc) - 1);

  // TODO: check for interrupts
  switch (op) {
    CASE(0x00, {}) // NOP
    CASE(0x01, { REG(bc) = fetch_word(cpu); }) // LD BC,d16
    CASE(0x02, { deref_store(cpu, REG(bc), REG(a)); }) // LD (BC),A
    CASE(0x03, { ++REG(bc); }) // INC BC
    CASE(0x04, { inc(cpu, &REG(b)); }) // INC B
    CASE(0x05, { dec(cpu, &REG(b)); }) // DEC B
    CASE(0x06, { REG(b) = fetch_byte(cpu); }) // LD B,d8
    CASE(0x0C, { inc(cpu, &REG(c)); }) // INC C
    CASE(0x0D, { dec(cpu, &REG(c)); }) // DEC C
    CASE(0x0E, { REG(c) = fetch_byte(cpu); }) // LD C,d8
    CASE(0x11, { REG(de) = fetch_word(cpu); }) // LD DE,d16
    CASE(0x12, { deref_store(cpu, REG(de), REG(a)); }) // LD (DE),A
    CASE(0x13, { ++REG(de); }) // INC DE
    CASE(0x14, { inc(cpu, &REG(d)); }) // INC D
    CASE(0x15, { dec(cpu, &REG(d)); }) // DEC D
    CASE(0x16, { REG(d) = fetch_byte(cpu); }) // LD D,d8
    CASE(0x17, {
      rotate_left(cpu, &REG(a));
      REG(f.z) = 0;
    }) // RLA
    CASE(0x18, { jump_relative(cpu); }) // JR r8
    CASE(0x1A, { REG(a) = deref_load(cpu, REG(de)); }) // LD A,(DE)
    CASE(0x1C, { inc(cpu, &REG(e)); }) // INC E
    CASE(0x1D, { dec(cpu, &REG(e)); }) // DEC E
    CASE(0x1E, { REG(e) = fetch_byte(cpu); }) // LD E,d8
    CASE(0x20, { conditional_jump_relative(cpu, !REG(f.z)); }) // JR NZ,r8
    CASE(0x21, { REG(hl) = fetch_word(cpu); }) // LD HL,d16
    CASE(0x22, { deref_store(cpu, REG(hl)++, REG(a)); }) // LD (HL+),A
    CASE(0x23, { ++REG(hl); }) // INC HL
    CASE(0x24, { inc(cpu, &REG(h)); }) // INC H
    CASE(0x26, { REG(h) = fetch_byte(cpu); }) // LD H,d8
    CASE(0x28, { conditional_jump_relative(cpu, REG(f.z)); }) // JR Z,r8
    CASE(0x2A, { REG(a) = deref_load(cpu, REG(hl)++); }) // LD A,(HL+)
    CASE(0x2C, { inc(cpu, &REG(l)); }) // INC L
    CASE(0x2D, { dec(cpu, &REG(l)); }) // DEC L
    CASE(0x2E, { REG(l) = fetch_byte(cpu); }) // LD L,d8
    CASE(0x31, { REG(sp) = fetch_word(cpu); }) // LD SP,d16
    CASE(0x32, { deref_store(cpu, REG(hl)--, REG(a)); }) // LD (HL-),A
    CASE(0x3D, { dec(cpu, &REG(a)); }) // DEC A
    CASE(0x3E, { REG(a) = fetch_byte(cpu); }) // LD A,d8
    CASE(0x46, { REG(b) = deref_load(cpu, REG(hl)); }) // LD B,(HL)
    CASE(0x47, { REG(b) = REG(a); }) // LD B,A
    CASE(0x4E, { REG(c) = deref_load(cpu, REG(hl)); }) // LD C,(HL)
    CASE(0x4F, { REG(c) = REG(a); }) // LD C,A
    CASE(0x56, { REG(d) = deref_load(cpu, REG(hl)); }) // LD D,(HL)
    CASE(0x57, { REG(d) = REG(a); }) // LD D,A
    CASE(0x67, { REG(h) = REG(a); }) // LD H,A
    CASE(0x77, { deref_store(cpu, REG(hl), REG(a)); }) // LD (HL),A
    CASE(0x78, { REG(a) = REG(b); }) // LD A,B
    CASE(0x7B, { REG(a) = REG(e); }) // LD A,E
    CASE(0x7C, { REG(a) = REG(h); }) // LD A,H
    CASE(0x7D, { REG(a) = REG(l); }) // LD A,L
    CASE(0x86, { add(cpu, deref_load(cpu, REG(hl)), 0); }) // ADD A,(HL)
    CASE(0x90, { REG(a) = subtract(cpu, REG(b), 0); }) // SUB B
    CASE(0xA9, { xor(cpu, REG(c)); }) // XOR C
    CASE(0xAE, { xor(cpu, deref_load(cpu, REG(hl))); }) // XOR (HL)
    CASE(0xAF, { xor(cpu, REG(a)); }) // XOR A
    CASE(0xB1, { or(cpu, REG(c)); }) // OR C
    CASE(0xB7, { or(cpu, REG(a)); }) // OR A
    CASE(0xBE, { subtract(cpu, deref_load(cpu, REG(hl)), 0); }) // CP (HL)
    CASE(0xC1, { REG(bc) = pop(cpu); }) // POP BC
    CASE(0xC3, { jump(cpu, fetch_word(cpu)); }) // JP a16
    CASE(0xC4, { call(cpu, !REG(f.z)); }) // CALL NZ,a16
    CASE(0xC5, {
      push(cpu, REG(bc));
      // This is weird
      cpu->tick_cycles += 4;
    }) // PUSH BC
    CASE(0xC6, { add(cpu, fetch_byte(cpu), 0); }) // ADD A,d8
    CASE(0xC9, { jump(cpu, pop(cpu)); }) // RET
    CASE(0xCB, { cb(cpu); }) // CB prefix
    CASE(0xCD, { call(cpu, 1); }) // CALL a16
    CASE(0xD5, {
      push(cpu, REG(de));
      // This is weird
      cpu->tick_cycles += 4;
    }) // PUSH DE
    CASE(0xD6, { REG(a) = subtract(cpu, fetch_byte(cpu), 0); }) // SUB d8
    CASE(0xE0, {
        deref_store(cpu, 0xFF00 | fetch_byte(cpu), REG(a));
    }) // LDH (a8),A
    CASE(0xE1, { REG(hl) = pop(cpu); }) // POP HL
    CASE(0xE2, { deref_store(cpu, 0xFF00 | REG(c), REG(a)); }) // LD (C),A
    CASE(0xE5, {
      push(cpu, REG(hl));
      // This is weird
      cpu->tick_cycles += 4;
    }) // PUSH HL
    CASE(0xE6, { and(cpu, fetch_byte(cpu)); }) // AND d8
    CASE(0xEA, { deref_store(cpu, fetch_word(cpu), REG(a)); }) // LD (a16),A
    CASE(0xF0, {
      REG(a) = deref_load(cpu, 0xFF00 | fetch_byte(cpu));
    }) // LDH A,(a8)
    CASE(0xF1, { REG(af) = pop(cpu); }) // POP AF
    CASE(0xF3, { cpu->interrupts_enabled = 0; }) // DI
    CASE(0xF5, {
      push(cpu, REG(af));
      // This is weird
      cpu->tick_cycles += 4;
    }) // PUSH AF
    CASE(0xFA, { REG(a) = deref_load(cpu, fetch_word(cpu)); }) // LD A,(a16)
    CASE(0xFB, { cpu->interrupts_enabled = 1; }) // EI
    CASE(0xFE, { subtract(cpu, fetch_byte(cpu), 0); }) // CP d8
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

static void cb(struct cpu* const cpu) {
  cpu->tick_cycles += 4;
  const uint8_t op = fetch_byte(cpu);
  switch (op) {
    CASE(0x7C, { bit(cpu, REG(h), 7); }) // BIT 7,H
    CASE(0x11, {
      rotate_left(cpu, &REG(c));
      REG(f.z) = !(REG(c));
    }) // RL C
    default:
      fprintf(stderr, "Unhandled 0xCB opcode: " PRIbyte "\n", op);
      exit(EXIT_FAILURE);
      break;
  }
}
