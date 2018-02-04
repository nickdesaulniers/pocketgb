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

// pack these to save space in the opcode table
enum __attribute__((packed)) kOpcode {
  kInvalid, // used for errors decoding
  kNop,
  kStop,
  kHalt,
  // load store
  kLoad,
  kPush,
  kPop,
  // inc/dec
  kInc,
  kDec,
  // rotates
  kRL,
  kRR,
  // math
  kAdd,
  kAddc,
  kSub,
  kSubc,
  kDAA,
  // logical
  kAnd,
  kXor,
  kOr,
  // comparison
  kCPL,
  kCp,
  // flags
  kSCF,
  kCCF,
  // jumps
  kJump,
  kCall,
  kRet,
  kRST,
  // prefix
  kCb,
  // Interrupts
  kEI,
  kDI,
};

static enum kOpcode decode_table [256] = {
  // 0x
  kNop, kLoad, kLoad, kInc, kInc, kDec, kLoad, kRL,
  kLoad, kAdd, kLoad, kDec, kInc, kDec, kLoad, kRR,
  // 1x
  kStop, kLoad, kLoad, kInc, kInc, kDec, kLoad, kRL,
  kJump, kAdd, kLoad, kDec, kInc, kDec, kLoad, kRR,
  // 2x
  kJump, kLoad, kLoad, kInc, kInc, kDec, kLoad, kDAA,
  kJump, kAdd, kLoad, kDec, kInc, kDec, kLoad, kCPL,
  // 3x
  kJump, kLoad, kLoad, kInc, kInc, kDec, kLoad, kSCF,
  kJump, kAdd, kLoad, kDec, kInc, kDec, kLoad, kCCF,
  // 4x
  kLoad, kLoad, kLoad, kLoad, kLoad, kLoad, kLoad, kLoad,
  kLoad, kLoad, kLoad, kLoad, kLoad, kLoad, kLoad, kLoad,
  // 5x
  kLoad, kLoad, kLoad, kLoad, kLoad, kLoad, kLoad, kLoad,
  kLoad, kLoad, kLoad, kLoad, kLoad, kLoad, kLoad, kLoad,
  // 6x
  kLoad, kLoad, kLoad, kLoad, kLoad, kLoad, kLoad, kLoad,
  kLoad, kLoad, kLoad, kLoad, kLoad, kLoad, kLoad, kLoad,
  // 7x
  kLoad, kLoad, kLoad, kLoad, kLoad, kLoad, kHalt, kLoad,
  kLoad, kLoad, kLoad, kLoad, kLoad, kLoad, kLoad, kLoad,
  // 8x
  kAdd, kAdd, kAdd, kAdd, kAdd, kAdd, kAdd, kAdd,
  kAddc, kAddc, kAddc, kAddc, kAddc, kAddc, kAddc, kAddc, kAddc,
  // 9x
  kSub, kSub, kSub, kSub, kSub, kSub, kSub, kSub, kSub,
  kSubc, kSubc, kSubc, kSubc, kSubc, kSubc, kSubc, kSubc,
  // Ax
  kAnd, kAnd, kAnd, kAnd, kAnd, kAnd, kAnd, kAnd,
  kXor, kXor, kXor, kXor, kXor, kXor, kXor, kXor,
  // Bx
  kOr, kOr, kOr, kOr, kOr, kOr, kOr, kOr,
  kCp, kCp, kCp, kCp, kCp, kCp, kCp, kCp,
  // Cx
  kRet, kPop, kJump, kJump, kCall, kPush, kAdd, kRST,
  kRet, kRet, kJump, kCb, kCall, kCall, kAddc, kRST,
  // Dx
  kRet, kPop, kJump, kInvalid, kCall, kPush, kSub, kRST,
  kRet, kRet, kJump, kInvalid, kCall, kInvalid, kSubc, kRST,
  // Ex
  kLoad, kPop, kLoad, kInvalid, kInvalid, kPush, kAnd, kRST,
  kAdd, kJump, kLoad, kInvalid, kInvalid, kInvalid, kXor, kRST,
  // Fx
  kLoad, kPop, kLoad, kDI, kInvalid, kPush, kOr, kRST,
  kLoad, kLoad, kLoad, kEI, kInvalid, kInvalid, kCp, kRST,
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
