#include <assert.h>
#include <stdio.h>
#include "lcd.h"

void transition (struct lcd* const lcd, const uint8_t mode) {
  printf("LCD: transition from %d to %d\n", lcd->mode, mode);
  if (mode == 1) {
    getchar();
  }
  lcd->mode = mode;
  lcd->cycles_in_current_mode = 0;
}

void update_line (struct lcd* const lcd, uint8_t line) {
  lcd->line = line;
  printf("LCD: advancing to line %d\n", line);
  wb(lcd->mmu, 0xFF44, line);
}

void update_lcd (struct lcd* const lcd, const uint8_t cycles) {
  // TODO: MMU needs to set this
  /*if (!lcd->enabled) {*/
    /*return;*/
  /*}*/

  lcd->total_cycles += cycles;
  lcd->cycles_in_current_mode += cycles;

  switch (lcd->mode) {
    // HBlank
    case 0:
      if (lcd->cycles_in_current_mode >= 204) {
        update_line(lcd, lcd->line + 1);

        if (lcd->line == 144) {
          transition(lcd, 1);
        } else {
          transition(lcd, 2);
        }
      }
      break;
    // VBlank
    case 1:
      if (lcd->cycles_in_current_mode >= 4560) {
        transition(lcd, 2);
        update_line(lcd, 0);
        lcd->line = 0;
      }
      break;
    case 2:
      if (lcd->cycles_in_current_mode >= 80) {
        transition(lcd, 3);
      }
      break;
    case 3:
      if (lcd->cycles_in_current_mode >= 172) {
        transition(lcd, 0);
      }
      // draw scanline?
      break;
    default:
      // Not a valid mode
      assert(false);
  }
}
