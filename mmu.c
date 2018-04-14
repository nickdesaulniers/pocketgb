#include "mmu.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"

static void handle_hardware_io_side_effects(struct mmu* const mem,
    const uint16_t addr, const uint8_t val);
static void handle_tile_write (const uint16_t addr);

uint8_t rb (const struct mmu* const mem, const uint16_t addr) {
  // todo: fancy case statement
  switch (addr & 0xF000) {
    case 0xE000:
      LOG(7, "read from echo ram\n");
      return rb(mem, addr - 0x2000);
    case 0xF000:
      switch (addr & 0x0F00) {
        case 0x0E00:
        case 0x0F00:
          break;
        default:
          LOG(7, "read from echo ram\n");
          return rb(mem, addr - 0x2000);
      }
      break;
  }
  return mem->memory[addr];
}

uint16_t rw (const struct mmu* const mem, const uint16_t addr) {
  return (rb(mem, addr + 1) << 8) | rb(mem, addr);
}

void wb (struct mmu* const mem, const uint16_t addr, const uint8_t val) {
  mem->memory[addr] = val;
  switch (addr & 0xF000) {
    case 0x8000:
    case 0x9000: // intentional fallthrough
      handle_tile_write(addr);
      mem->tile_data_dirty = 1;
      break;
    case 0xE000:
      // echo ram
      LOG(7, "write to echo ram\n");
      return wb(mem, addr - 0x2000, val);
    case 0xF000:
      switch (addr & 0x0F00) {
        case 0x0E00:
          break;
        case 0x0F00:
          handle_hardware_io_side_effects(mem, addr, val);
          break;
        default:
          // echo ram
          LOG(7, "write to echo ram\n");
          return wb(mem, addr - 0x2000, val);
      }
      break;
  }
}

void ww (struct mmu* const mem, const uint16_t addr, const uint16_t val) {
  wb(mem, addr + 1, val >> 8);
  wb(mem, addr, (const uint8_t) (val & 0xFF));
}

// return 0 on error
// truncates down to size_t, though fseek returns a long
static size_t get_filesize (FILE* f) {
  if (fseek(f, 0L, SEEK_END) != 0) {
    return 0;
  }
  long fsize = ftell(f);
  if (fsize <= 0 || (size_t)fsize > SIZE_MAX) {
    return 0;
  }
  rewind(f);
  return fsize;
}

// return 0 on success
static int read_file_into_memory (const char* const path, void* dest,
    size_t* const rom_size) {
  printf("opening %s\n", path);
  FILE* f = fopen(path, "r");
  if (!f) {
    fprintf(stderr, "failed to open %s\n", path);
    goto error;
  }
  size_t fsize = get_filesize(f);
  if (!fsize) {
    fprintf(stderr, "%s appears to be an empty file\n", path);
    goto fclose;
  }
  if (rom_size) {
    *rom_size = fsize;
  }
  // only the first 0x7FFF bytes get mapped in, otherwise a memory bank
  // controller must be used
  fsize = fsize < 0x7FFF ? fsize : 0x7FFF;

  size_t read = fread(dest, 1, fsize, f);
  if (read != fsize) goto fclose;
  return fclose(f);
fclose:
  fclose(f);
error:
  return -1;
}

// http://gameboy.mongenel.com/dmg/asmmemmap.html
// Returns NULL on error
// bios may be null
// rom must not be
struct mmu* init_memory (const char* const restrict bios,
    const char* const restrict rom) {
  assert(rom != NULL);
  struct mmu* const mmu = malloc(sizeof(struct mmu));
  if (!mmu) goto error;
#ifndef NDEBUG
  memset(mmu->memory, 0xF7, sizeof(mmu->memory));
#endif
  int rc = read_file_into_memory(rom, mmu->memory, &mmu->rom_size);
  if (rc) goto free;
  if (bios) {
    memcpy(mmu->rom_masked_by_bios, mmu->memory, sizeof(mmu->rom_masked_by_bios));
    rc = read_file_into_memory(bios, mmu->memory, NULL);
    if (rc) goto free;
  }
  mmu->has_bios = !!bios;
  mmu->tile_data_dirty = 1;
  wb(mmu, 0xFF04, 0x00); // DIV
  wb(mmu, 0xFF05, 0x00); // TIMA
  wb(mmu, 0xFF06, 0x00); // TMA
  wb(mmu, 0xFF07, 0x00); // TAC
  wb(mmu, 0xFF0F, 0x00); // IF
  wb(mmu, 0xFFFF, 0x00); // IE
  return mmu;
free:
  free(mmu);
error:
  return NULL;
}

void deinit_memory (struct mmu* const mem) {
  if (mem) {
    free(mem);
  }
}


static void power_up_sequence (struct mmu* const mem) {
  // remove the BIOS
  memcpy(mem, mem->rom_masked_by_bios, 256);
  wb(mem, 0xFF10, 0x80); // NR10
  wb(mem, 0xFF11, 0xBF); // NR11
  wb(mem, 0xFF12, 0xF3); // NR12
  wb(mem, 0xFF14, 0xBF); // NR14
  wb(mem, 0xFF21, 0x3F); // NR21
  wb(mem, 0xFF17, 0x00); // NR22
  wb(mem, 0xFF19, 0xBF); // NR24
  wb(mem, 0xFF1A, 0x7F); // NR30
  wb(mem, 0xFF1B, 0xFF); // NR31
  wb(mem, 0xFF1C, 0x9F); // NR32
  wb(mem, 0xFF1E, 0xBF); // NR33
  wb(mem, 0xFF20, 0xFF); // NR41
  wb(mem, 0xFF21, 0x00); // NR42
  wb(mem, 0xFF22, 0x00); // NR43
  // TODO: errata in http://bgb.bircd.org/pandocs.htm#powerupsequence?
  // NR30 seems to be both 0xFF1A and 0xFF23 ???
  wb(mem, 0xFF23, 0xBF); // NR30
  wb(mem, 0xFF24, 0x77); // NR50
  wb(mem, 0xFF25, 0xF3); // NR51
  // TODO: different based on model
  wb(mem, 0xFF26, 0xF1); // NR52
  wb(mem, 0xFF40, 0x91); // LCDC
  wb(mem, 0xFF42, 0x00); // SCY
  wb(mem, 0xFF43, 0x00); // SCX
  wb(mem, 0xFF45, 0x00); // LYC
  wb(mem, 0xFF47, 0xFC); // BGP
  wb(mem, 0xFF48, 0xFF); // OBP0
  wb(mem, 0xFF49, 0xFF); // OBP1
  wb(mem, 0xFF4A, 0x00); // WY
  wb(mem, 0xFF4B, 0x00); // WX
  wb(mem, 0xFFFF, 0x00); // IE
}

// if 1XXX,XXXX is written to 0xFF02, start transfer of 0xFF01
static void sc_write (struct mmu* const mem, const uint8_t val) {
  if (val & 0x80) {
    /*LOG(1, "putting " PRIbyte "\n", rb(mem, 0xFF01));*/
    /*putchar(rb(mem, 0xFF01));*/
    /*printf("%c\n", rb(mem, 0xFF01));*/
    putchar(rb(mem, 0xFF01));
  } else {
    LOG(8, "not putting\n");
  }
}

static void handle_hardware_io_side_effects(struct mmu* const mem,
    const uint16_t addr, const uint8_t val) {
  switch (addr) {
    case 0xFF01:
      LOG(7, "data written to SB " PRIbyte " " PRIshort "\n", val, addr);
      break;
    case 0xFF02:
      LOG(7, "data written to SC " PRIbyte " " PRIshort "\n", val, addr);
      sc_write(mem, val);
      break;
    case 0xFF04:
      LOG(7, "data written to DIV " PRIbyte " " PRIshort "\n", val, addr);
      mem->memory[addr] = 0;
      break;
    case 0xFF0F:
      LOG(7, "data written to IF " PRIbyte " @ " PRIshort "\n", val, addr);
      break;
    case 0xFF40:
      LOG(7, "write to LCDC: %d\n", val);
      break;
    case 0xFF50:
      // TODO: check val
      LOG(7, "write to 0xFF50");
      power_up_sequence(mem);
      break;
    case 0xFFFF:
      LOG(7, "data written to IE " PRIbyte " @ " PRIshort "\n", val, addr);
      break;
    default:
      break;
  }
}


static void handle_tile_write (const uint16_t addr) {
  if (addr <= 0x87FF) {
    LOG(4, "write to tile set #1 %X\n", addr);
  } else if (addr <= 0x8FFF) {
    LOG(4, "write to tile set #1 or set #0 %X\n", addr);
  } else if (addr <= 0x97FF) {
    LOG(4, "write to tile set #0 %X\n", addr);
  } else if (addr <= 0x9BFF) {
    LOG(4, "write to tile map #0 %X\n", addr);
  } else {
    LOG(4, "write to tile map #1 %X\n", addr);
  }
}
