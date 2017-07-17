#include <assert.h>
#include <stdio.h>
#include "lcd.h"

void transition (struct lcd* const lcd, const uint8_t mode) {
  printf("LCD: transition from %d to %d\n", lcd->mode, mode);
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

static uint8_t get_pallete_number (const int bit_pos, const uint8_t low,
    const uint8_t high) {
  // Wont work for bit_pos 0, since we'd be right shifting by a negative number
  assert(0 < bit_pos && bit_pos < 8);
  /*return ((high & (1 << 7)) >> 6) | ((low & (1 << 7)) >> 7);*/
  return ((high & (1 << bit_pos)) >> (bit_pos - 1)) |
    ((low & (1 << bit_pos)) >> bit_pos);
}

// takes a row of 8 pixels
static void shade_tile_row (uint8_t* const pixels, const uint8_t low,
    const uint8_t high) {
  // I drew this out to understand it
  /*pixels[0] = ((high & (1 << 7)) >> 6) | ((low & (1 << 7)) >> 7);*/
  /*pixels[1] = ((high & (1 << 6)) >> 5) | ((low & (1 << 6)) >> 6);*/
  /*// ...*/
  /*pixels[6] = ((high & (1 << 1)) >> 0) | ((low & (1 << 1)) >> 1);*/
  /*pixels[7] = ((high & (1 << 0)) << 1) | ((low & (1 << 0)) >> 0);*/
  for (int i = 0; i < 7; ++i) {
    pixels[i] = get_pallete_number(7 - i, low, high);
    printf("%d", pixels[i]);
  }
  pixels[7] = ((high & (1 << 0)) << 1) & ((low & (1 << 0)) >> 0);
  printf("%d\n", pixels[7]);
}

static void shade_tile (const struct mmu* const mem, const uint16_t base) {
  uint8_t pixels [8];

  for (uint16_t addr = base; addr < (base + 16); addr += 2) {
    const uint8_t low = rb(mem, addr);
    const uint8_t high = rb(mem, addr + 1);
    /*printf("== 0x%X 0x%X ==\n", low, high);*/

    // TODO: palette translation
    assert(sizeof(pixels) == 8);
    shade_tile_row(pixels, low, high);
  }
  printf("== end tile 0x%X==\n", base);
}

// http://www.huderlem.com/demos/gameboy2bpp.html
void debug_draw_tilemap (const struct lcd* const lcd,
    SDL_Renderer* const renderer) {
  const struct mmu* const mem = lcd->mmu;

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

  for (uint16_t addr = 0x8000; addr < 0x8FFF; addr += 2) {
  /*for (uint16_t addr = 0x8800; addr < 0x97FF; addr += 2) {*/
    shade_tile(mem, addr);
  }
  getchar();
}
