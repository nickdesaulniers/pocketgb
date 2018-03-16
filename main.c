#include <assert.h>
#include <signal.h>
#include <stdio.h>

#include "SDL.h"
#include "SDL_video.h"

#include "cpu.h"
#include "lcd.h"
#include "logging.h"
#include "window_list.h"

static int should_exit = 0;
static void catch_sig_int(int signum) {
  should_exit = signum == SIGINT;
}

static int initialize_system (const char* const restrict bios,
    const char* const restrict rom, struct cpu* const restrict cpu,
    struct lcd* const restrict lcd) {
  assert(rom != NULL);
  assert(cpu != NULL);
  struct mmu* const mmu = init_memory(bios, rom);
  if (!mmu) return -1;
  // TODO: registers get initialized differently based on model
  init_cpu(cpu, mmu);
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
  int rc = 0;
  if (argc == 2) {
    // If just the bios is passed, init_cpu will look at rom size and not jump
    // the pc forward.
    rc = initialize_system(NULL, argv[1], &cpu, &lcd);
  } else {
    rc = initialize_system(argv[1], argv[2], &cpu, &lcd);
  }
  if (rc) {
    fprintf(stderr, "Failed to initialize system.\n");
    return -1;
  }
  if(signal(SIGINT, catch_sig_int) == SIG_ERR) {
    perror("Unable to set SIGINT handler.\n");
  }

  /*assert(SDL_Init(SDL_INIT_VIDEO) == 0);*/
  /*struct window_list* window_list_head = NULL;*/
  /*create_debug_windows(&window_list_head);*/
  /*SDL_Event e;*/

  // TODO: while cpu not halted
  while (1) {
    /*SDL_PollEvent(&e);*/
    /*if (should_exit || e.type == SDL_QUIT) {*/
    if (should_exit) {
      break;
    }

    int cycles = tick_once(&cpu);
    update_lcd(&lcd, cycles);
  }

  /*window_list_deinit(window_list_head);*/
  /*SDL_Quit();*/
  deinit_memory(cpu.mmu);
  printf("exiting cleanly\n");
}
