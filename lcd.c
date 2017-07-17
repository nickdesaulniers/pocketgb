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

static uint8_t get_palette_number (const int bit_pos, const uint8_t low,
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
    pixels[i] = get_palette_number(7 - i, low, high);
    printf("%d", pixels[i]);
  }
  pixels[7] = ((high & (1 << 0)) << 1) & ((low & (1 << 0)) >> 0);
  printf("%d\n", pixels[7]);
}

static void shade_tile (const struct mmu* const mem, uint8_t* const pixels,
    const uint16_t base) {
  uint8_t* p = &pixels[0];

  for (uint16_t addr = base; addr < (base + 16); addr += 2) {
    const uint8_t low = rb(mem, addr);
    const uint8_t high = rb(mem, addr + 1);

    // TODO: palette translation
    shade_tile_row(p, low, high);
    p += 8;
  }
  printf("== end tile 0x%X==\n", base);
}

void paint_pixels (const uint8_t* const pixels, SDL_Renderer* const renderer,
    const int offset_x, const int offset_y) {
  for (int x = 0; x < 8; ++x) {
    for (int y = 0; y < 8; ++y) {
      if (pixels[y * 8 + x]) {
        // TODO: palette translation
        SDL_RenderDrawPoint(renderer, x + offset_x, y + offset_y);
      }
    }
  }
}

// http://www.huderlem.com/demos/gameboy2bpp.html
void debug_draw_tilemap (const struct lcd* const lcd,
    SDL_Renderer* const renderer) {
  // Tiles are 8px x 8px == 64
  uint8_t pixels [64];
  const struct mmu* const mem = lcd->mmu;
  int x = 0;
  int y = 0;

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

  for (uint16_t addr = 0x8000; addr < 0x8FFF; addr += 2) {
  /*for (uint16_t addr = 0x8800; addr < 0x97FF; addr += 2) {*/
    shade_tile(mem, pixels, addr);
    // pixels is now shaded
    // 16 x 16 == 256 total tiles
    paint_pixels(pixels, renderer, x, y);
    x += 8;
    if (x == 128) {
      y += 8;
      x = 0;
    }
  }
  SDL_RenderPresent(renderer);
  getchar();
}
