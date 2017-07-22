#include <assert.h>
#include "window_list.h"

void window_list_init (struct window_list* const wl) {
  wl->window = NULL;
  wl->next = NULL;
}

void window_list_deinit (struct window_list* wl) {
  assert(wl && wl->window);
  SDL_DestroyWindow(wl->window);
  wl = wl->next;
  while (wl && wl->window) {
    SDL_DestroyWindow(wl->window);
    struct window_list* prev = wl;
    wl = wl->next;
    free(prev);
  }
}

void window_list_insert (struct window_list* wl, SDL_Window* window) {
  assert(window != NULL);

  // initial insert
  if (!wl->window) {
    wl->window = window;
    return;
  }

  // allocate new node
  struct window_list* new_wl = malloc(sizeof(struct window_list));
  new_wl->window = window;
  new_wl->next = NULL;

  // find the tail
  // tail->next == NULL
  while (wl->next) {
    wl = wl->next;
  }
  wl->next = new_wl;
}
