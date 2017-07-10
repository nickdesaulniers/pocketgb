#include <stdio.h>
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

// return 0 on error
// truncates down to size_t, though fseek returns a long
static size_t get_filesize (FILE* f) {
  if (fseek(f, 0L, SEEK_END) != 0) {
    return 0;
  }
  long fsize = ftell(f);
  if (fsize > SIZE_MAX || fsize <= 0) {
    return 0;
  }
  rewind(f);
  return fsize;
}

// return 0 on success
static int read_file_into_memory (const char* const path, void* dest) {
  printf("opening %s\n", path);
  FILE* f = fopen(path, "r");
  if (!f) {
    return -1;
  }
  size_t fsize = get_filesize(f);
  if (!fsize) {
    return -1;
  }

  size_t read = fread(dest, 1, fsize, f);
  if (read != fsize) {
    return -1;
  }

  if (fclose(f)) {
    return -1;
  }
}

// http://gameboy.mongenel.com/dmg/asmmemmap.html
struct mmu* init_memory (char** roms, const int len) {
  struct mmu* const mem = malloc(sizeof(struct mmu));
  if (mem) {
    // poison value
    memset(mem, 0xF7, sizeof(struct mmu));
  }

  for (int i = len; i; --i) {
    if (read_file_into_memory(roms[i - 1], mem)) {
      free(mem);
      return NULL;
    }
  }

  return mem;
}

void deinit_memory (struct mmu* const mem) {
  if (mem) {
    free(mem);
  }
}
