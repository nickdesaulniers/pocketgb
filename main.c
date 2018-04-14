#include <assert.h>
#include <signal.h>
#include <stdio.h>

#include "SDL.h"
#include "SDL_video.h"

#include "cpu.h"
#include "lcd.h"
#include "timer.h"
#include "logging.h"

static int should_exit = 0;
static void catch_sig_int(int signum) {
  should_exit = signum == SIGINT;
}

static int initialize_system (const char* const restrict bios,
    const char* const restrict rom, struct cpu* const restrict cpu,
    struct lcd* const restrict lcd, struct timer* const timer) {
  assert(rom != NULL);
  assert(cpu != NULL);
  struct mmu* const mmu = init_memory(bios, rom);
  if (!mmu) return -1;
  // TODO: registers get initialized differently based on model
  init_cpu(cpu, mmu);
  init_timer(timer, mmu);
  // TODO: init function
  lcd->mmu = mmu;
  lcd->mode = 2;
  return 0;
}

int main (int argc, char** argv) {
  if (argc < 2 || argc > 3) {
    fprintf(stderr, "USAGE: ./pocketgb [bios.gb] <rom.gb>\n");
    return -1;
  }

  struct cpu cpu = { 0 };
  struct lcd lcd = { 0 };
  struct timer timer;
  int rc = 0;
  if (argc == 2) {
    // If just the bios is passed, init_cpu will look at rom size and not jump
    // the pc forward.
    rc = initialize_system(NULL, argv[1], &cpu, &lcd, &timer);
  } else {
    rc = initialize_system(argv[1], argv[2], &cpu, &lcd, &timer);
  }
  if (rc) {
    fprintf(stderr, "Failed to initialize system.\n");
    return -1;
  }
  if(signal(SIGINT, catch_sig_int) == SIG_ERR) {
    perror("Unable to set SIGINT handler.\n");
  }

  assert(SDL_Init(SDL_INIT_VIDEO) == 0);
  struct windows windows;
  create_debug_windows(&windows);
  SDL_Event e;

  // TODO: while cpu not halted
  while (1) {
    SDL_PollEvent(&e);
    if (should_exit || e.type == SDL_QUIT) {
      break;
    }

    tick_once(&cpu);
    timer_tick(&timer, cpu.tick_cycles);
    handle_interrupts(&cpu);
    update_lcd(&lcd, cpu.tick_cycles);
    update_debug_windows(&windows, &lcd);
  }

  destroy_windows(&windows);
  SDL_Quit();
  deinit_memory(cpu.mmu);
  printf("\nexiting cleanly\n");
}
