#pragma once

#include <stdint.h>
// http://gameboy.mongenel.com/dmg/asmmemmap.html
struct mmu {
  uint8_t memory [65536];
};

struct mmu* init_memory (char** roms, const int len);
void deinit_memory (struct mmu* const);
uint8_t rb (const struct mmu* const mem, uint16_t addr);
uint16_t rw (const struct mmu* const mem, uint16_t addr);
void wb (struct mmu* const mem, const uint16_t addr, const uint8_t val);
void ww (struct mmu* const mem, const uint16_t addr, const uint16_t val);
