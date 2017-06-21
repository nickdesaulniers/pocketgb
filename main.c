#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cpu.h"

// returns 0 on success
// TODO: fseek and compare fread size
int read_rom_into_memory (const char* const path, uint8_t* const memory) {
  FILE* f = fopen(path, "r");
  if (f) {
     fread(memory, 1, 256, f);
     fclose(f);
     return 0;
  }
  return -1;
}

int main (int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "USAGE: ./pocketgb <rom.gb>\n");
    return -1;
  }

  // TODO: malloc
  uint8_t memory [65536];
  // poison value
  memset(&memory, 0xF7, sizeof(memory));
  // TODO: registers get initialized differently based on model
  struct cpu lr35902 = {0};
  lr35902.memory = memory;

  int rc = read_rom_into_memory(argv[1], &memory[0]);
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
