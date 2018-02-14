#include "lcd.h"

#include <assert.h>
#include <stdio.h>

#include "SDL_render.h"

#include "logging.h"

static void transition (struct lcd* const lcd, const uint8_t mode) {
  LOG(5, "LCD: transition from %d to %d\n", lcd->mode, mode);
  lcd->mode = mode;
  lcd->cycles_in_current_mode = 0;
}

static int is_lcd_on (const struct lcd* const lcd) {
  const uint8_t lcdc = rb(lcd->mmu, 0xFF40);
  return !!(lcdc & (1 << 7));
}

static void update_line (struct lcd* const lcd, const uint8_t cycles) {
  lcd->cycles_in_current_line += cycles;

  if (lcd->cycles_in_current_line >= 456) {
    lcd->cycles_in_current_line = 0;
    // TODO: modulo?
    ++lcd->line;
    if (lcd->line == 154) {
      lcd->line = 0;
    }
    LOG(5, "LCD: advancing to line %d\n", lcd->line);
    wb(lcd->mmu, 0xFF44, lcd->line);
  }
}

// http://gameboy.mongenel.com/dmg/gbc_lcdc_timing.txt`
void update_lcd (struct lcd* const lcd, const uint8_t cycles) {
  if (!is_lcd_on(lcd)) {
    return;
  }

  lcd->total_cycles += cycles;
  lcd->cycles_in_current_mode += cycles;
  update_line(lcd, cycles);

  switch (lcd->mode) {
    // HBlank
    case 0:
      if (lcd->cycles_in_current_mode >= 204) {

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
  printf("active tileset: %d\n", !!(lcdc & (1 << 4)));
  return !!(lcdc & (1 << 4));
}

static int bg_active_tilemap (const struct lcd* const lcd) {
  const uint8_t lcdc = rb(lcd->mmu, 0xFF00);
  printf("active tilemap: %d\n", !!(lcdc & (1 << 3)));
  return !!(lcdc & (1 << 3));
}

static void paint_bg_tilemap (uint8_t* map_data,
    const struct lcd* const lcd) {

  const int active_tilemap = bg_active_tilemap(lcd);
  const uint16_t base = active_tilemap ? 0x9C00 : 0x9800;
  const uint16_t top = active_tilemap ? 0x9FFF : 0x9BFF;
  for (uint16_t addr = base; addr < top; addr += 32) {
    for (int x = 0; x < 32; ++x) {
      *map_data = rb(lcd->mmu, addr + x);
      /*printf("%d |", *map_data);*/
      ++map_data;
    }
    /*puts("");*/
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
        /*printf("%d", *tile_data);*/
        ++tile_data;
      }
      *tile_data = ((high & (1 << 0)) << 1) | ((low & (1 << 0)) >> 0);
      /*printf("%d\n", *tile_data);*/
      ++tile_data;
    }
    /*puts("");*/
  }
}

// renderer agnostic
static void paint_tile (const uint8_t* tile_data, SDL_Renderer* const renderer,
    int dx, int dy) {

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

static const uint8_t* seek_tile (const uint8_t* tile_data, unsigned int i) {
  // 256 tiles in total
  assert(i < 256);
  // 8px x 8px per tile
  return tile_data + i * 64;
}

static void paint_tiles (const uint8_t* const tile_data,
    SDL_Renderer* const renderer) {

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

  // 256 tiles in total
  for (int tile = 0; tile < 256; ++tile) {
    // 16 rows, 16 columns, 8px per tile
    int dx = (tile % 16) * 8;
    int dy = (tile / 16) * 8;
    paint_tile(seek_tile(tile_data, tile), renderer, dx, dy);
  }

  SDL_RenderPresent(renderer);
}

static void map_tiles (const uint8_t* const map_data,
    const uint8_t* const tile_data, SDL_Renderer* const renderer) {

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  for (int map = 0; map < 32 * 32; ++map) {
    int dx = (map % 32) * 8;
    int dy = (map / 32) * 8;
    paint_tile(seek_tile(tile_data, map_data[map]), renderer, dx, dy);
    /*if (map == 261) {*/
    /*if (map_data[map]) {*/
      /*printf("XXX: %d\n", map);*/
      /*break;*/
    /*};*/
  }
  SDL_RenderPresent(renderer);
}

static void perror_sdl (const char* const msg) {
  fprintf(stderr, "%s: %s\n", msg, SDL_GetError());
}

static SDL_Renderer* get_cleared_renderer (SDL_Window* const window) {
  if (!window) {
    perror_sdl("unable to open window");
    return NULL;
  }
  SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
  if (!renderer) {
    perror_sdl("unable to create renderer");
    // TODO: close window?
    return NULL;
  }
  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
  SDL_RenderClear(renderer);
  SDL_RenderPresent(renderer);
  return renderer;
}

void create_debug_windows (struct window_list** window_list_head) {
  SDL_Window* debug_windows [] = {
    SDL_CreateWindow("Debug Tileset",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 16 * 8, 16 * 8, 0),
    SDL_CreateWindow("Debug Tilemapped Tiles",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 32 * 8, 32 * 8, 0)
  };
  for (unsigned int i = 0; i < sizeof(debug_windows) / sizeof(SDL_Window*); ++i) {
    SDL_Renderer* renderer = get_cleared_renderer(debug_windows[i]);
    assert(renderer != NULL);
    window_list_insert(window_list_head, debug_windows[i], renderer);
  }
}

// http://www.huderlem.com/demos/gameboy2bpp.html
void update_debug_windows (const struct window_list* const window_list_head,
    const struct lcd* const lcd) {
  assert(window_list_head->next != NULL);
  assert(window_list_head->next->renderer != NULL);
  assert(window_list_head->renderer != window_list_head->next->renderer);

  uint8_t* const tile_data = calloc(8 * 8 * 256, sizeof(uint8_t));
  shade_tiles(tile_data, lcd);
  paint_tiles(tile_data, window_list_head->renderer);

  uint8_t* const map_data = calloc(32 * 32, sizeof(uint8_t));
  paint_bg_tilemap(map_data, lcd);
  map_tiles(map_data, tile_data, window_list_head->next->renderer);

  free(tile_data);
  free(map_data);
}
