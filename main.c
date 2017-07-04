#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cpu.h"

// returns 0 on success
// TODO: fseek and compare fread size
int read_rom_into_memory (const char* const path, struct mmu* const mem) {
  FILE* f = fopen(path, "r");
  if (!f) {
    return -1;
  }
  const size_t rom_size = 256;
  const size_t bytes_read = fread(mem->memory, 1, rom_size, f);
  fclose(f);
  return bytes_read != rom_size;
}

int main (int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "USAGE: ./pocketgb <rom.gb>\n");
    return -1;
  }

  struct mmu* memory = init_memory();
  // TODO: registers get initialized differently based on model
  struct cpu lr35902 = {0};
  lr35902.mmu = memory;

  int rc = read_rom_into_memory(argv[1], memory);
  if (rc != 0) {
    perror(argv[1]);
    return -1;
  }

  // TODO: while cpu not halted
  while (1) {
    instr i = decode(&lr35902);
    i(&lr35902);
    puts("===");
  }
}
