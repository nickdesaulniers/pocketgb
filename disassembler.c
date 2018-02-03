#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static void pbyte (const uint8_t b) {
  printf("0x%02hhX\n", b);
}

// return 0 on error
// truncates down to size_t, though fseek returns a long
static size_t get_filesize (FILE* const f) {
  if (fseek(f, 0L, SEEK_END) != 0) return 0;
  const long fsize = ftell(f);
  if (fsize > SIZE_MAX || fsize <= 0) return 0;
  rewind(f);
  return fsize;
}

// return NULL on error
// dynamically allocates return value
static void* read_file_into_memory (const char* const path, size_t* const fsize) {
  FILE* const f = fopen(path, "r");
  if (!f) goto err;

  *fsize = get_filesize(f);
  if (!*fsize) goto fclose;

  void* const dest = malloc(*fsize);
  if (!dest) goto fclose;

  const size_t read = fread(dest, 1, *fsize, f);
  if (read != *fsize) goto free;

  if (fclose(f)) goto free;

  return dest;
free:
  free(dest);
fclose:
  fclose(f);
err:
  return NULL;
}

struct rom {
  uint8_t* data;
  char* path;
  size_t size;
};

// dynamically allocates return value, and return value ->data
static struct rom* read_rom (const char* const path) {
  struct rom* rom = malloc(sizeof(struct rom));
  if (!rom) return NULL;

  rom->data = read_file_into_memory(path, &rom->size);
  rom->path = (char*)path;
  return rom;
}

static void destroy_rom (struct rom* const rom) {
  free(rom->data);
  free(rom);
}

enum kOpcode {
  kInvalid, // used for errors decoding
  kNop,
  kStop,
  kLoad,
  kInc,
  kDec,
  kRL,
  kRR,
  kAdd,
  kJump,
};

/*enum kCbOpcode {};*/

struct operand {};

struct instruction {
  char* name;
  uint8_t* rom_addr; // where in the rom we found this
  enum kOpcode opcode;
  /*enum kCbOpcode cb_opcode;*/
  struct operand operands [2];
  char instruction_length;
};

static enum kOpcode decode_table [256] = {
  // 0x
  kNop, kLoad, kLoad, kInc, kInc, kDec, kLoad, kRL,
  kLoad, kAdd, kLoad, kDec, kInc, kDec, kLoad, kRR,
  // 1x
  kStop, kLoad, kLoad, kInc, kInc, kDec, kLoad, kRL,
  kJump, kAdd, kLoad, kDec, kInc, kDec, kLoad, kRR,
  // 2x
  // 3x
  // 4x
  // 5x
  // 6x
  // 7x
  // 8x
  // 9x
  // Ax
  // Bx
  // Cx
  // Dx
  // Ex
  // Fx
};

static enum kOpcode decode_opcode (const uint8_t* const data, const size_t pc) {
  const uint8_t first_byte = data[pc];
  const enum kOpcode opcode = decode_table[first_byte];
  return kInvalid;
}

int disassemble (const struct rom* const rom) {
  size_t pc = 0;
  while (pc < rom->size) {
    // index into rom
    printf("pc: %zu\n", pc);
    decode_opcode(rom->data, pc);
    /*const uint8_t first_byte = rom->data[pc];*/
    /*printf("read: ");*/
    /*pbyte(first_byte);*/
    break; //
  }
  return 0;
}

int main (int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: ./disassembler <rom.gb>\n");
    return -1;
  }

  const char* const rom_fname = argv[1];
  printf("opening %s\n", rom_fname);
  const struct rom* const rom = read_rom(rom_fname);
  if (!rom) {
    fprintf(stderr, "Error opening rom file %s\n", rom_fname);
    return -1;
  }
  disassemble(rom);
  destroy_rom((struct rom* const)rom);
}
