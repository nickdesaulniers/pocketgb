#include <stdint.h>
#include <stdbool.h>
#include "mmu.h"
#include "window_list.h"

struct lcd {
  struct mmu* mmu;
  uint32_t total_cycles; // for debugging?
  uint16_t cycles_in_current_mode;
  uint16_t cycles_in_current_line;
  uint8_t mode;
  uint8_t line;
  bool enabled;
};

void update_lcd (struct lcd* const lcd, const uint8_t cycles);
void create_debug_windows (struct window_list** window_list_head);
void update_debug_windows (const struct window_list* window_list_head,
    const struct lcd* const lcd);
