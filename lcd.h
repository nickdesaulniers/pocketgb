#include <stdint.h>
#include <stdbool.h>
#include "SDL_render.h"
#include "mmu.h"

struct lcd {
  struct mmu* mmu;
  uint32_t total_cycles; // for debugging?
  uint16_t cycles_in_current_mode;
  uint8_t mode;
  uint8_t line;
  bool enabled;
};

void update_lcd (struct lcd* const lcd, const uint8_t cycles);
void debug_draw_tilemap (const struct lcd* const lcd, SDL_Renderer* const renderer);
