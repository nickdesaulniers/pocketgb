#include "cpu.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "logging.h"

#define REG(x) cpu->registers.x

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
  deref_store(cpu, REG(sp)--, value >> 8);
  deref_store(cpu, REG(sp)--, value & 0xFF);
}
static uint8_t get_bit(const uint8_t src, const uint8_t index) {
  assert(index < 8);
  return (src >> index) & 1U;
}
static uint8_t test_half_carry(const uint8_t x) {
  return (x & 0x10) == 0x10;
}

// logical operations
static void inc(struct cpu* const cpu, uint8_t* const x) {
  ++*x;
  REG(f.z) = !*x;
  REG(f.n) = 0;
  REG(f.h) = test_half_carry(*x);
}
static void xor (struct cpu * const cpu, const uint8_t x) {
  REG(a) ^= x;
  REG(f.z) = !REG(a);
  REG(f.n) = REG(f.h) = REG(f.c) = 0;
} static void bit(struct cpu* const cpu, const uint8_t x, const uint8_t index) {
  REG(f.z) = get_bit(x, index);
  REG(f.n) = 0;
  REG(f.h) = 1;
}
static void rotate_left(struct cpu* const cpu, uint8_t* const x) {
  const uint8_t carry = get_bit(7, *x);
  *x = (*x << 1) | REG(f.c);
  // REG(f.z) is handled different when destination register is A
  REG(f.c) = carry;
  REG(f.h) = REG(f.n) = 0;
}

uint8_t tick_once(struct cpu* const cpu) {
#ifndef NDEBUG
  const uint16_t pre_op_pc = REG(pc);
#endif
  cpu->tick_cycles = 0;
  const uint8_t op = fetch_byte(cpu);

  // TODO: check for interrupts
  switch (op) {
    case 0x00:
      // NOP
      break;
    case 0x06:
      // LD B,d8
      REG(b) = fetch_byte(cpu);
      break;
    case 0x0C:
      // INC C
      inc(cpu, &REG(c));
      break;
    case 0x0E:
      // LD C,d8
      REG(c) = fetch_byte(cpu);
      break;
    case 0x11:
      // LD DE,d16
      REG(de) = fetch_word(cpu);
      break;
    case 0x1A:
      // LD A,(DE)
      REG(a) = deref_load(cpu, REG(de));
      break;
    case 0x20:
      // LD (BC),A
      deref_store(cpu, REG(bc), REG(a));
      break;
    case 0x21:
      // LD DE,d16
      REG(sp) = fetch_word(cpu);
      break;
    case 0x26:
      // LD H,d8
      REG(h) = fetch_byte(cpu);
      break;
    case 0x31:
      // LD SP,d16
      REG(sp) = fetch_word(cpu);
      break;
    case 0x32:
      // LD (HL-),A
      deref_store(cpu, REG(hl)--, REG(a));
      break;
    case 0x3E:
      // LD A,d8
      REG(a) = fetch_byte(cpu);
      break;
    case 0x4F:
      // LD C,A
      REG(c) = REG(a);
      break;
    case 0x77:
      // LD (HL),A
      deref_store(cpu, REG(hl), REG(a));
    case 0xAF:
      // XOR A
      xor(cpu, REG(a));
      break;
    case 0xC3:
      // JP a16
      jump(cpu, fetch_word(cpu));
      break;
    case 0xC5:
      // PUSH BC
      push(cpu, REG(bc));
      // This is weird
      cpu->tick_cycles += 4;
      break;
    case 0xCB:
      cb(cpu);
      break;
    case 0xCD:
      // CALL a16
      /*printf("count of cycles for CALL a16: %d\n", cpu->tick_cycles);*/
      push(cpu, REG(pc) + 2);
      /*printf("count of cycles for CALL a16: %d\n", cpu->tick_cycles);*/
      jump(cpu, fetch_word(cpu));
      /*printf("count of cycles for CALL a16: %d\n", cpu->tick_cycles);*/
      break;
    case 0xE0:
      // LDH (a8),A
      deref_store(cpu, 0xFF00 + fetch_byte(cpu), REG(a));
      break;
    case 0xE2:
      // LD (C),A
      deref_store(cpu, 0xFF00 + REG(c), REG(a));
      break;
    case 0xFB:
      // EI
      cpu->interrupts_enabled = 1;
      break;
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
    // TODO: at the end of dmg bios, looks like af is 0x0101
    REG(af) = 0x01B0;
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
    case 0x7C:
      // BIT 7,H
      bit(cpu, REG(h), 7);
      break;
    case 0x11:
      // RL C
      break;
    default:
      fprintf(stderr, "Unhandled 0xCB opcode: " PRIbyte "\n", op);
      exit(EXIT_FAILURE);
      break;
  }
}
