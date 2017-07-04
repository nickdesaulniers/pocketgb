#include <stdlib.h>
#include <string.h>

#include "mmu.h"

uint8_t rb (const struct mmu* const mem, const uint16_t addr) {
  // todo: fancy case statement
  return mem->memory[addr];
}

uint16_t rw (const struct mmu* const mem, const uint16_t addr) {
  return (rb(mem, addr + 1) << 8) | rb(mem, addr);
}

void wb (struct mmu* const mem, const uint16_t addr, const uint8_t val) {
  mem->memory[addr] = val;
}

void ww (struct mmu* const mem, const uint16_t addr, const uint16_t val) {
  wb(mem, addr + 1, val >> 8);
  wb(mem, addr, (const uint8_t) (val & 0xFF));
}

// http://gameboy.mongenel.com/dmg/asmmemmap.html
struct mmu* init_memory () {
  struct mmu* const mem = malloc(sizeof(struct mmu));
  if (mem) {
    // poison value
    memset(mem, 0xF7, sizeof(struct mmu));
  }
  return mem;
}

void deinit_memory (struct mmu* const mem) {
  if (mem) {
    free(mem);
  }
}
