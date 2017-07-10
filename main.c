#include <stdio.h>

#include "cpu.h"
#include "lcd.h"

int main (int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "USAGE: ./pocketgb <bios.gb> <rom.gb>\n");
    return -1;
  }

  struct mmu* memory = init_memory(&argv[1], argc - 1);
  // TODO: registers get initialized differently based on model
  struct cpu lr35902 = {0};
  lr35902.mmu = memory;

  // TODO: init function
  struct lcd lcd = { 0 };
  lcd.mmu = memory;
  lcd.mode = 2;

  // TODO: while cpu not halted
  while (1) {
    instr i = decode(&lr35902);
    i(&lr35902);
    puts("===");
    // TODO: return actual timings from instructions
    update_lcd(&lcd, 4);
  }

  deinit_memory(memory);
}
