#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "SDL_render.h"
#include "SDL_video.h"
#include "mmu.h"

struct lcd {
  struct mmu* mmu;
  uint32_t total_cycles; // for debugging?
  uint16_t cycles_in_current_mode;
  uint16_t cycles_in_current_line;
  uint8_t mode;
  uint8_t line;
  bool enabled;
};

struct winren {
  SDL_Window* window;
  SDL_Renderer* renderer;
};

struct windows {
  struct winren main;
  struct winren tiles;
  struct winren tilemap;
};

void update_lcd (struct lcd* const lcd, const uint8_t cycles);
void create_debug_windows (struct windows* const windows);
void update_debug_windows (struct windows* const windows,
    const struct lcd* const lcd);
void destroy_windows (struct windows* windows);
