#pragma once

#include <stdint.h>

#include "cpu.h"

// http://gameboy.mongenel.com/dmg/asmmemmap.html
struct mmu {
  uint8_t memory [65536];
  // the BIOS covers this until write to 0xFF50
  uint8_t rom_masked_by_bios [256];
  struct cpu* cpu;
};

struct mmu* init_memory (char** roms, const int len);
void deinit_memory (struct mmu* const);
uint8_t rb (const struct mmu* const mem, uint16_t addr);
uint16_t rw (const struct mmu* const mem, uint16_t addr);
void wb (struct mmu* const mem, const uint16_t addr, const uint8_t val);
void ww (struct mmu* const mem, const uint16_t addr, const uint16_t val);
