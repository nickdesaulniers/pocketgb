#include <assert.h>
#include <stdio.h>

#include "SDL.h"
#include "SDL_video.h"

#include "cpu.h"
#include "lcd.h"

void perror_sdl (const char* const msg) {
  fprintf(stderr, "%s: %s\n", msg, SDL_GetError());
}

bool breakpoint (const struct cpu* const cpu, const uint16_t pc_addr) {
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

  // SDL
  assert(SDL_Init(SDL_INIT_VIDEO) == 0);
  // TODO: SDL_CreateWindowAndRenderer
  SDL_Window* window = SDL_CreateWindow("pocketgb", SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED, 640, 480, 0);
  if (!window) {
    perror_sdl("unable to open window");
    return -1;
  }
  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
  if (!renderer) {
    perror_sdl("unable to create renderer");
    return -1;
  }

  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  SDL_RenderClear(renderer);
  SDL_RenderPresent(renderer);
  /*SDL_Delay(5000);*/
  SDL_Event e;

  // TODO: while cpu not halted
  while (1) {
    SDL_PollEvent(&e);
    if (e.type == SDL_QUIT) {
      break;
    }

    instr i = decode(&lr35902);
    i(&lr35902);
    puts("===");
    // TODO: return actual timings from instructions
    update_lcd(&lcd, 4);

    if (breakpoint(&lr35902, 0x0055)) {
      puts("printing tilemap");
      debug_draw_tilemap(&lcd, renderer);
    }
  }

  SDL_DestroyWindow(window);
  SDL_Quit();

  deinit_memory(memory);
}
