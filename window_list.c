#include "window_list.h"

#include <assert.h>

void window_list_deinit (struct window_list* wl) {
  assert(wl && wl->window);
  while (wl && wl->window) {
    SDL_DestroyWindow(wl->window);
    SDL_DestroyRenderer(wl->renderer);
    struct window_list* prev = wl;
    wl = wl->next;
    free(prev);
  }
}

void window_list_insert (struct window_list** wl, SDL_Window* window,
    SDL_Renderer* renderer) {

  assert(wl != NULL);
  assert(window != NULL);
  assert(renderer != NULL);

  // allocate new node
  struct window_list* new_wl = malloc(sizeof(struct window_list));
  new_wl->window = window;
  new_wl->renderer = renderer;
  new_wl->next = NULL;

  if (*wl == NULL) {
    *wl = new_wl;
    return;
  }

  // find the tail
  // tail->next == NULL
  struct window_list* temp = *wl;
  while (temp->next) {
    temp = temp->next;
  }
  temp->next = new_wl;
}
