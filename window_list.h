#pragma once

#include "SDL_video.h"

struct window_list {
  SDL_Window* window;
  struct window_list* next;
};

void window_list_init (struct window_list* const wl);
void window_list_deinit (struct window_list* wl);
void window_list_insert (struct window_list* wl, SDL_Window* window);
