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
enum __attribute__((packed)) Opcode {
  kInvalid, // used for errors decoding
  kNOP,
  kSTOP,
  kHALT,
  // Loads (stores are just loads where dest is dereferenced).
  // Most loads are register to register, so encode length == 1.  When a
  // literal is passed, it may be a longer load due to the size of the literal.
  kLD,
  kPUSH,
  kPOP,
  // inc/dec
  kINC,
  kDEC,
  // rotates
  kRL,
  kRLC,
  kRR,
  kRRC,
  // math
  kADD,
  kADC,
  kSUB,
  kSBC,
  kCP,
  // conversion
  kDAA,
  // logical
  kAND,
  kXOR,
  kOR,
  kCPL,
  // flags
  kSCF,
  kCCF,
  // jumps
  kJP,
  kCALL,
  kRET,
  kRST,
  // prefix
  kCB,
  // Interrupts
  kEI,
  kDI,
};

static const enum Opcode decode_table [256] = {
  // 0x
  kNOP, kLD, kLD, kINC, kINC, kDEC, kLD, kRLC,
  kLD, kADD, kLD, kDEC, kINC, kDEC, kLD, kRRC,
  // 1x
  kSTOP, kLD, kLD, kINC, kINC, kDEC, kLD, kRL,
  kJP, kADD, kLD, kDEC, kINC, kDEC, kLD, kRR,
  // 2x
  kJP, kLD, kLD, kINC, kINC, kDEC, kLD, kDAA,
  kJP, kADD, kLD, kDEC, kINC, kDEC, kLD, kCPL,
  // 3x
  kJP, kLD, kLD, kINC, kINC, kDEC, kLD, kSCF,
  kJP, kADD, kLD, kDEC, kINC, kDEC, kLD, kCCF,
  // 4x
  kLD, kLD, kLD, kLD, kLD, kLD, kLD, kLD,
  kLD, kLD, kLD, kLD, kLD, kLD, kLD, kLD,
  // 5x
  kLD, kLD, kLD, kLD, kLD, kLD, kLD, kLD,
  kLD, kLD, kLD, kLD, kLD, kLD, kLD, kLD,
  // 6x
  kLD, kLD, kLD, kLD, kLD, kLD, kLD, kLD,
  kLD, kLD, kLD, kLD, kLD, kLD, kLD, kLD,
  // 7x
  kLD, kLD, kLD, kLD, kLD, kLD, kHALT, kLD,
  kLD, kLD, kLD, kLD, kLD, kLD, kLD, kLD,
  // 8x
  kADD, kADD, kADD, kADD, kADD, kADD, kADD, kADD,
  kADC, kADC, kADC, kADC, kADC, kADC, kADC, kADC,
  // 9x
  kSUB, kSUB, kSUB, kSUB, kSUB, kSUB, kSUB, kSUB,
  kSBC, kSBC, kSBC, kSBC, kSBC, kSBC, kSBC, kSBC,
  // Ax
  kAND, kAND, kAND, kAND, kAND, kAND, kAND, kAND,
  kXOR, kXOR, kXOR, kXOR, kXOR, kXOR, kXOR, kXOR,
  // Bx
  kOR, kOR, kOR, kOR, kOR, kOR, kOR, kOR,
  kCP, kCP, kCP, kCP, kCP, kCP, kCP, kCP,
  // Cx
  kRET, kPOP, kJP, kJP, kCALL, kPUSH, kADD, kRST,
  kRET, kRET, kJP, kCB, kCALL, kCALL, kADC, kRST,
  // Dx
  kRET, kPOP, kJP, kInvalid, kCALL, kPUSH, kSUB, kRST,
  kRET, kRET, kJP, kInvalid, kCALL, kInvalid, kSBC, kRST,
  // Ex
  kLD, kPOP, kLD, kInvalid, kInvalid, kPUSH, kAND, kRST,
  kADD, kJP, kLD, kInvalid, kInvalid, kInvalid, kXOR, kRST,
  // Fx
  kLD, kPOP, kLD, kDI, kInvalid, kPUSH, kOR, kRST,
  kLD, kLD, kLD, kEI, kInvalid, kInvalid, kCP, kRST
};


enum Operand {
  kNONE,
  // 8b
  kA,
  kB,
  kC,
  kDEREF_C, // more like *(0xFF00 + C)
  kD,
  kE,
  kH,
  kL,
  // 16b
  kAF,
  kBC,
  kDEREF_BC,
  kDE,
  kDEREF_DE,
  kHL,
  kDEREF_HL,
  kDEREF_HL_INC,
  kDEREF_HL_DEC,
  kSP,
  // Literals
  kd8,
  kr8,
  kDEREF_a8,
  kDEREF_a16,
};
static const enum Operand operand_0_table [256] = {
  // 0x
  kNONE, kBC, kDEREF_BC, kBC, kB, kB, kB, kNONE,
  kDEREF_a16, kHL, kA, kBC, kC, kC, kC, kNONE,
  // 1x
  kNONE, kDE, kDEREF_DE, kDE, kD, kD, kD, kNONE,
  kr8, kHL, kA, kDE, kE, kE, kE, kNONE,
  // 2x
  kNONE, kHL, kDEREF_HL_INC, kHL, kH, kH, kH, kNONE,
  kNONE, kHL, kA, kHL, kL, kL, kL, kNONE,
  // 3x
  kNONE, kSP, kDEREF_HL_DEC, kSP, kDEREF_HL, kDEREF_HL, kDEREF_HL, kNONE,
  kNONE, kHL, kA, kSP, kA, kA, kA, kNONE,
  // 4x
  kB, kB, kB, kB, kB, kB, kB, kB,
  kC, kC, kC, kC, kC, kC, kC, kC,
  // 5x
  kD, kD, kD, kD, kD, kD, kD, kD,
  kE, kE, kE, kE, kE, kE, kE, kE,
  // 6x
  kH, kH, kH, kH, kH, kH, kH, kH,
  kL, kL, kL, kL, kL, kL, kL, kL,
  // 7x
  kDEREF_HL, kDEREF_HL, kDEREF_HL, kDEREF_HL,
  kDEREF_HL, kDEREF_HL, kNONE, kDEREF_HL,
  kA, kA, kA, kA, kA, kA, kA, kA,
  // 8x
  kA, kA, kA, kA, kA, kA, kA, kA,
  kA, kA, kA, kA, kA, kA, kA, kA,
  // 9x
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kA, kA, kA, kA, kA, kA, kA, kA,
  // Ax
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // Bx
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // Cx
  kNONE, kBC, kNONE, kNONE, kNONE, kBC, kA, kNONE,
  kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kA, kNONE,
  // Dx
  kNONE, kDE, kNONE, kNONE, kNONE, kDE, kNONE, kNONE,
  kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kA, kNONE,
  // Ex
  kDEREF_a8, kHL, kDEREF_C, kNONE, kNONE, kHL, kd8, kNONE,
  kSP, kDEREF_HL, kDEREF_a16, kNONE, kNONE, kNONE, kd8, kNONE,
  // Fx
  kA, kAF, kA, kNONE, kNONE, kAF, kd8, kNONE,
  kHL, kSP, kA, kNONE, kNONE, kNONE, kd8, kNONE
};

struct instruction {
  char* name;
  uint8_t* rom_addr; // where in the rom we found this
  enum Opcode opcode;
  /*enum CbOpcode cb_opcode;*/
  /*struct operand operands [2];*/
  char instruction_length;
};

static enum Opcode decode_opcode (const uint8_t* const data, const size_t pc) {
  const uint8_t first_byte = data[pc];
  const enum Opcode opcode = decode_table[first_byte];
  return kInvalid;
}

int disassemble (const struct rom* const rom) {
  size_t pc = 0;
  while (pc < rom->size) {
    // index into rom
    printf("pc: %zu\n", pc);
    const enum Opcode opcode = decode_opcode(rom->data, pc);
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
