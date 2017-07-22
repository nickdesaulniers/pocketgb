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
  /*const uint8_t lcdc = rb(lcd->mmu, 0xFF40);*/
  /*// make sure the lcd is on*/
  /*assert(lcdc & (1 << 7));*/

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

// AKA BG & Window Tile Data Select
static int bg_active_tileset (const struct lcd* const lcd) {
  const uint8_t lcdc = rb(lcd->mmu, 0xFF00);
  return !!(lcdc & (1 << 4));
}

static int bg_active_tilemap (const struct lcd* const lcd) {
  const uint8_t lcdc = rb(lcd->mmu, 0xFF00);
  return !!(lcdc & (1 << 3));
}

static void print_bg_tilemap (const struct lcd* const lcd) {
  const int active_tilemap = bg_active_tilemap(lcd);
  const uint16_t base = active_tilemap ? 0x9C00 : 0x9800;
  const uint16_t top = active_tilemap ? 0x9FFF : 0x9BFF;
  for (uint16_t addr = base; addr < top; addr += 32) {
    for (int x = 0; x < 32; ++x) {
      int8_t tile_index = rb(lcd->mmu, addr + x);
      printf("%d |", tile_index);
    }
    puts("");
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

static void shade_tiles (uint8_t* tile_data, const struct lcd* const lcd) {
  const int active_tileset = bg_active_tileset(lcd);
  const uint16_t base = active_tileset ? 0x8000 : 0x8800;
  const uint16_t top = active_tileset ? 0x8FFF : 0x97FF;

  for (uint16_t addr = base; addr < top; addr += 16) {
    const uint16_t ttop = addr + 16;
    // one tile
    for (uint16_t taddr = addr; taddr < ttop; taddr += 2) {
      // one row
      const uint8_t low = rb(lcd->mmu, taddr);
      const uint8_t high = rb(lcd->mmu, taddr + 1);
      for (int i = 0; i < 7; ++i) {
        *tile_data = get_palette_number(7 - i, low, high);
        printf("%d", *tile_data);
        ++tile_data;
      }
      *tile_data = ((high & (1 << 0)) << 1) | ((low & (1 << 0)) >> 0);
      printf("%d\n", *tile_data);
      ++tile_data;
    }
    puts("");
    // TODO: only print 25 tiles for now
    /*if (addr == base + 16 * 25) {*/
      /*break;*/
    /*}*/
  }
}

static void paint_tiles (const uint8_t* tile_data,
    SDL_Renderer* const renderer) {

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

  for (int tile = 0; tile < 256; ++tile) {
    int dx = (tile % 16) * 8;
    int dy = (tile / 16) * 8;
    // draw one tile
    for (int sy = 0; sy < 8; ++sy) {
      int tdx = dx;
      // draw only first row
      for (int sx = 0; sx < 8; ++sx) {
        if (tile_data[sx]) {
          SDL_RenderDrawPoint(renderer, tdx, dy);
        }
        ++tdx;
      }
      tile_data += 8;
      ++dy;
    }
  }
}


// http://www.huderlem.com/demos/gameboy2bpp.html
void debug_draw_tilemap (const struct lcd* const lcd,
    SDL_Renderer* const renderer) {

  uint8_t* const tile_data = calloc(8 * 8 * 256, sizeof(uint8_t));
  shade_tiles(tile_data, lcd);
  paint_tiles(tile_data, renderer);
  SDL_RenderPresent(renderer);

  print_bg_tilemap(lcd);
  free(tile_data);
  getchar();
}
