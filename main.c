#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cpu.h"
#include "lcd.h"

// return 0 on error
// truncates down to size_t, though fseek returns a long
size_t get_filesize (FILE* f) {
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
int read_file_into_memory (const char* const path, void* dest) {
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

int main (int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "USAGE: ./pocketgb <bios.gb> <rom.gb>\n");
    return -1;
  }

  // TODO: leaks memory
  struct mmu* memory = init_memory();
  // TODO: registers get initialized differently based on model
  struct cpu lr35902 = {0};
  lr35902.mmu = memory;

  for (int i = argc - 1; i > 0; --i) {
    int rc = read_file_into_memory(argv[i], memory);
    if (rc != 0) {
      perror(argv[i]);
      return -1;
    }
  }

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
}
