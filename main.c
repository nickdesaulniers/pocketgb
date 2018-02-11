#include <assert.h>
#include <stdio.h>

#include "SDL.h"
#include "SDL_video.h"

#include "cpu.h"
#include "lcd.h"
#include "window_list.h"

static bool breakpoint (const struct cpu* const cpu, const uint16_t pc_addr) {
  int should_break = cpu->registers.pc == pc_addr;
  /*if (should_break) {*/
    /*printf("breaking at 0x%X\n", pc_addr);*/
    /*getchar();*/
  /*}*/
  return should_break;
}

int main (int argc, char** argv) {
  if (argc < 3) {
    fprintf(stderr, "USAGE: ./pocketgb <bios.gb> <rom.gb>\n");
    return -1;
  }

  struct mmu* memory = init_memory(&argv[1], argc - 1);
  // TODO: registers get initialized differently based on model
  struct cpu lr35902 = { 0 };
  lr35902.mmu = memory;
  memory->cpu = &lr35902;

  // TODO: init function
  struct lcd lcd = { 0 };
  lcd.mmu = memory;
  lcd.mode = 2;

  assert(SDL_Init(SDL_INIT_VIDEO) == 0);
  struct window_list* window_list_head = NULL;
  create_debug_windows(&window_list_head);
  SDL_Event e;

  // TODO: while cpu not halted
  while (1) {
    SDL_PollEvent(&e);
    if (e.type == SDL_QUIT) {
      break;
    }

    instr i = decode(&lr35902);
    uint16_t pre_op_pc = lr35902.registers.pc;
    i(&lr35902);
    // otherwise i forgot to update pc
    assert(pre_op_pc != lr35902.registers.pc);
    puts("===");
    // TODO: return actual timings from instructions
    update_lcd(&lcd, 4);

    if (breakpoint(&lr35902, 0x0055)) {
    /*if (breakpoint(&lr35902, 0x27f7)) {*/
      puts("printing tilemap");
      update_debug_windows(window_list_head, &lcd);
    }
  }

  window_list_deinit(window_list_head);
  SDL_Quit();
  deinit_memory(memory);
}
