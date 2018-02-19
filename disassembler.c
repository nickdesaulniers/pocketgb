#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"

// return 0 on error
// truncates down to size_t, though fseek returns a long
static size_t get_filesize (FILE* const f) {
  if (fseek(f, 0L, SEEK_END) != 0) return 0;
  const long fsize = ftell(f);
  if (fsize <= 0 || (size_t)fsize > SIZE_MAX ) return 0;
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
  // rotates (sometimes prefixed)
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
  kJR,
  kCALL,
  kRET,
  kRETI,
  kRST,
  // Interrupts
  kEI,
  kDI,
  // prefix
  kCB,
  // prefixed
  kBIT,
  kSLA,
  kSRA,
  kSRL,
  kSWAP,
  kRES,
  kSET
};

static const char* const opcode_str_table [] = {
  "INVALID",
  "NOP",
  "STOP",
  "HALT",
  "LD",
  "PUSH",
  "POP",
  "INC",
  "DEC",
  "RL",
  "RLC",
  "RR",
  "RRC",
  "ADD",
  "ADC",
  "SUB",
  "SBC",
  "CP",
  "DAA",
  "AND",
  "XOR",
  "OR",
  "CPL",
  "SCF",
  "CCF",
  "JP",
  "JR",
  "CALL",
  "RET",
  "RETI",
  "RST",
  "EI",
  "DI",
  "CB",
  "BIT",
  "SLA",
  "SRA",
  "SRL",
  "SWAP",
  "RES",
  "SET"
};

enum __attribute__((packed)) Operand {
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
  // conditions (only used by JP)
  kCOND_Z,
  kCOND_NZ,
  kCOND_C,
  kCOND_NC,
  // literal bit positions, used by 0xCB instructions
  kBIT_0,
  kBIT_1,
  kBIT_2,
  kBIT_3,
  kBIT_4,
  kBIT_5,
  kBIT_6,
  kBIT_7,
  // literals used by RST instructions
  kLITERAL_0x00,
  kLITERAL_0x08,
  kLITERAL_0x10,
  kLITERAL_0x18,
  kLITERAL_0x20,
  kLITERAL_0x28,
  kLITERAL_0x30,
  kLITERAL_0x38,
  // Operands that appear after this enum add one to the instruction length.
  // Literals
  kd8,
  kr8,
  kSP_r8,
  kDEREF_a8,
  // Operands that appear after this enum add two to the instruction length.
  kd16,
  ka16,
  kDEREF_a16,
};

static const char* const operand_str_table [] = {
  "",
  "A",
  "B",
  "C",
  "(C)",
  "D",
  "E",
  "H",
  "L",
  "AF",
  "BC",
  "(BC)",
  "DE",
  "(DE)",
  "HL",
  "(HL)",
  "(HL+)",
  "(HL-)",
  "SP",
  "Z",
  "NZ",
  "C",
  "NC",
  "0",
  "1",
  "2",
  "3",
  "4",
  "5",
  "6",
  "7",
  "0x00",
  "0x08",
  "0x10",
  "0x18",
  "0x20",
  "0x28",
  "0x30",
  "0x38",
  "d8",
  "r8",
  "SP+r8",
  "(a8)",
  "d16",
  "a16",
  "(a16)"
};

struct instruction {
  enum Opcode opcode;
  enum Operand operands [2];
  // length in bytes for this instruction
  uint8_t length;
};

static const struct instruction instruction_table [256] = {
  // 0x
  { kNOP, { kNONE, kNONE }, 1 }, { kLD, { kBC, kd16 }, 3 },
  { kLD, { kDEREF_BC, kA }, 1 }, { kINC, { kBC, kNONE }, 1 },
  { kINC, { kB, kNONE }, 1 }, { kDEC, { kB, kNONE }, 1 },
  { kLD, { kB, kd8 }, 2 }, { kRLC, { kA, kNONE }, 1 },

  { kLD, { kDEREF_a16, kSP }, 3 }, { kADD, { kHL, kBC }, 1 },
  { kLD, { kA, kDEREF_BC }, 1 }, { kDEC, { kBC, kNONE }, 1 },
  { kINC, { kC, kNONE }, 1 }, { kDEC, { kC, kNONE }, 1 },
  { kLD, { kC, kd8 }, 2 }, { kRRC, { kA, kNONE }, 1 },
  // 1x
  { kSTOP, { kNONE, kNONE }, 2 }, { kLD, { kDE, kd16 }, 3 },
  { kLD, { kDEREF_DE, kA }, 1 }, { kINC, { kDE, kNONE }, 1 },
  { kINC, { kD, kNONE }, 1 }, { kDEC, { kD, kNONE }, 1 },
  { kLD, { kD, kd8 }, 2 }, { kRL, { kA, kNONE }, 1 },

  { kJR, { kNONE, kr8 }, 2 }, { kADD, { kHL, kDE }, 1 },
  { kLD, { kA, kDEREF_DE }, 1 }, { kDEC, { kDE, kNONE }, 1 },
  { kINC, { kE, kNONE }, 1 }, { kDEC, { kE, kNONE }, 1},
  { kLD, { kE, kd8 }, 2 }, { kRR, { kA, kNONE}, 1 },
  // 2x
  { kJR, { kCOND_NZ, kr8 }, 2 }, { kLD, { kHL, kd16 }, 3 },
  { kLD, { kDEREF_HL_INC, kA }, 1 }, { kINC, { kHL, kNONE }, 1 },
  { kINC, { kH, kNONE }, 1 }, { kDEC, { kH, kNONE }, 1 },
  { kLD, { kH, kd8 }, 2 }, { kDAA, { kNONE, kNONE }, 1 },

  { kJR, { kCOND_Z, kr8 }, 2 }, { kADD, { kHL, kHL }, 1 },
  { kLD, { kA, kDEREF_HL_INC }, 1 }, { kDEC, { kHL, kNONE }, 1 },
  { kINC, { kL, kNONE }, 1 }, { kDEC, { kL, kNONE }, 1 },
  { kLD, { kL, kd8 }, 2 }, { kCPL, { kNONE, kNONE }, 1 },
  // 3x
  { kJR, { kCOND_NC, kr8 }, 2 }, { kLD, { kSP, kd16 }, 3 },
  { kLD, { kDEREF_HL_DEC, kA }, 1 }, { kINC, { kSP, kNONE }, 1 },
  { kINC, { kDEREF_HL, kNONE }, 1 }, { kDEC, { kDEREF_HL, kNONE }, 1 },
  { kLD, { kDEREF_HL, kd8 }, 2 }, { kSCF, { kNONE, kNONE }, 1 },

  { kJR, { kCOND_C, kr8 }, 2 }, { kADD, { kHL, kSP }, 1 },
  { kLD, { kA, kDEREF_HL_DEC }, 1 }, { kDEC, { kSP, kNONE }, 1 },
  { kINC, { kA, kNONE }, 1 }, { kDEC, { kA, kNONE }, 1 },
  { kLD, { kA, kd8 }, 2 }, { kCCF, { kNONE, kNONE }, 1 },
  // 4x
  { kLD, { kB, kB }, 1 }, { kLD, { kB, kC }, 1 },
  { kLD, { kB, kD }, 1 }, { kLD, { kB, kE }, 1 },
  { kLD, { kB, kH }, 1 }, { kLD, { kB, kL }, 1 },
  { kLD, { kB, kDEREF_HL }, 1 }, { kLD, { kB, kA }, 1 },

  { kLD, { kC, kB }, 1 }, { kLD, { kC, kC }, 1 },
  { kLD, { kC, kD }, 1 }, { kLD, { kC, kE }, 1 },
  { kLD, { kC, kH }, 1 }, { kLD, { kC, kL }, 1 },
  { kLD, { kC, kDEREF_HL }, 1 }, { kLD, { kC, kA }, 1 },
  // 5x
  { kLD, { kD, kB }, 1 }, { kLD, { kD, kC }, 1 },
  { kLD, { kD, kD }, 1 }, { kLD, { kD, kE }, 1 },
  { kLD, { kD, kH }, 1 }, { kLD, { kD, kL }, 1 },
  { kLD, { kD, kDEREF_HL }, 1 }, { kLD, { kD, kA }, 1 },

  { kLD, { kE, kB }, 1 }, { kLD, { kE, kC }, 1 },
  { kLD, { kE, kD }, 1 }, { kLD, { kE, kE }, 1 },
  { kLD, { kE, kH }, 1 }, { kLD, { kE, kL }, 1 },
  { kLD, { kE, kDEREF_HL }, 1 }, { kLD, { kE, kA }, 1 },
  // 6x
  { kLD, { kH, kB }, 1 }, { kLD, { kH, kC }, 1 },
  { kLD, { kH, kD }, 1 }, { kLD, { kH, kE }, 1 },
  { kLD, { kH, kH }, 1 }, { kLD, { kH, kL }, 1 },
  { kLD, { kH, kDEREF_HL }, 1 }, { kLD, { kH, kA }, 1 },

  { kLD, { kL, kB }, 1 }, { kLD, { kL, kC }, 1 },
  { kLD, { kL, kD }, 1 }, { kLD, { kL, kE }, 1 },
  { kLD, { kL, kH }, 1 }, { kLD, { kL, kL }, 1 },
  { kLD, { kL, kDEREF_HL }, 1 }, { kLD, { kL, kA }, 1 },
  // 7x
  { kLD, { kDEREF_HL, kB }, 1 }, { kLD, { kDEREF_HL, kC }, 1 },
  { kLD, { kDEREF_HL, kD }, 1 }, { kLD, { kDEREF_HL, kE }, 1 },
  { kLD, { kDEREF_HL, kH }, 1 }, { kLD, { kDEREF_HL, kL }, 1 },
  { kHALT, { kNONE, kNONE }, 1 }, { kLD, { kDEREF_HL, kA }, 1 },

  { kLD, { kA, kB }, 1 }, { kLD, { kA, kC }, 1 },
  { kLD, { kA, kD }, 1 }, { kLD, { kA, kE }, 1 },
  { kLD, { kA, kH }, 1 }, { kLD, { kA, kL }, 1 },
  { kLD, { kA, kDEREF_HL }, 1 }, { kLD, { kA, kA }, 1 },
  // 8x
  { kADD, { kA, kB }, 1 }, { kADD, { kA, kC }, 1 },
  { kADD, { kA, kD }, 1 }, { kADD, { kA, kE }, 1 },
  { kADD, { kA, kH }, 1 }, { kADD, { kA, kL }, 1 },
  { kADD, { kA, kDEREF_HL }, 1 }, { kADD, { kA, kA }, 1 },

  { kADC, { kA, kB }, 1 }, { kADC, { kA, kC }, 1 },
  { kADC, { kA, kD }, 1 }, { kADC, { kA, kE }, 1 },
  { kADC, { kA, kH }, 1 }, { kADC, { kA, kL }, 1 },
  { kADC, { kA, kDEREF_HL }, 1 }, { kADC, { kA, kA }, 1 },
  // 9x
  // TODO: while the kA dest is implied, maybe it makes things simpler to
  // always list it as the first operand?
  { kSUB, { kB, kNONE }, 1 }, { kSUB, { kC, kNONE }, 1 },
  { kSUB, { kD, kNONE }, 1 }, { kSUB, { kE, kNONE }, 1 },
  { kSUB, { kH, kNONE }, 1 }, { kSUB, { kL, kNONE }, 1 },
  { kSUB, { kDEREF_HL, kNONE }, 1 }, { kSUB, { kA, kNONE }, 1 },

  { kSBC, { kA, kB }, 1 }, { kSBC, { kA, kC }, 1 },
  { kSBC, { kA, kD }, 1 }, { kSBC, { kA, kE }, 1 },
  { kSBC, { kA, kH }, 1 }, { kSBC, { kA, kL }, 1 },
  { kSBC, { kA, kDEREF_HL }, 1 }, { kSBC, { kA, kA }, 1 },
  // Ax
  { kAND, { kB, kNONE }, 1 }, { kAND, { kC, kNONE }, 1 },
  { kAND, { kD, kNONE }, 1 }, { kAND, { kE, kNONE }, 1 },
  { kAND, { kH, kNONE }, 1 }, { kAND, { kL, kNONE }, 1 },
  { kAND, { kDEREF_HL, kNONE }, 1 }, { kAND, { kA, kNONE }, 1 },

  { kXOR, { kB, kNONE }, 1 }, { kXOR, { kC, kNONE }, 1 },
  { kXOR, { kD, kNONE }, 1 }, { kXOR, { kE, kNONE }, 1 },
  { kXOR, { kH, kNONE }, 1 }, { kXOR, { kL, kNONE }, 1 },
  { kXOR, { kDEREF_HL, kNONE }, 1 }, { kXOR, { kA, kNONE }, 1 },
  // Bx
  { kOR, { kB, kNONE }, 1 }, { kOR, { kC, kNONE }, 1 },
  { kOR, { kD, kNONE }, 1 }, { kOR, { kE, kNONE }, 1 },
  { kOR, { kH, kNONE }, 1 }, { kOR, { kL, kNONE }, 1 },
  { kOR, { kDEREF_HL, kNONE }, 1 }, { kOR, { kA, kNONE }, 1 },

  { kCP, { kB, kNONE }, 1 }, { kCP, { kC, kNONE }, 1 },
  { kCP, { kD, kNONE }, 1 }, { kCP, { kE, kNONE }, 1 },
  { kCP, { kH, kNONE }, 1 }, { kCP, { kL, kNONE }, 1 },
  { kCP, { kDEREF_HL, kNONE }, 1 }, { kCP, { kA, kNONE }, 1 },
  // Cx
  { kRET, { kCOND_NZ, kNONE }, 1 }, { kPOP, { kBC, kNONE }, 1 },
  { kJP, { kCOND_NZ, ka16 }, 3 }, { kJP, { kNONE, ka16 }, 3 },
  { kCALL, { kCOND_NZ, ka16 }, 3 }, { kPUSH , { kBC, kNONE }, 1 },
  { kADD, { kA, kd8 }, 2 }, { kRST, { kLITERAL_0x00, kNONE }, 1 },

  { kRET, { kCOND_Z, kNONE }, 1 }, { kRET, { kNONE, kNONE }, 1 },
  { kJP, { kCOND_Z, ka16 }, 3 }, { kCB, { kNONE, kNONE }, 2 },
  { kCALL, { kCOND_Z, ka16 }, 3 }, { kCALL, { kNONE, ka16 }, 3 },
  { kADC, { kA, kd8 }, 2 }, { kRST, { kLITERAL_0x08, kNONE }, 1 },
  // Dx
  { kRET, { kCOND_NC, kNONE }, 1 }, { kPOP, { kDE, kNONE }, 1 },
  { kJP, { kCOND_NC, ka16 }, 3 }, { kInvalid, { kNONE, kNONE }, 1 },
  { kCALL, { kCOND_NC, ka16 }, 3 }, { kPUSH, { kDE, kNONE }, 1 },
  { kSUB, { kNONE, kd8 }, 2 }, { kRST, { kLITERAL_0x10, kNONE }, 1 },

  { kRET, { kCOND_C, kNONE }, 1 }, { kRETI, { kNONE, kNONE }, 1 },
  { kJP, { kCOND_C, ka16 }, 3 }, { kInvalid, { kNONE, kNONE }, 1 },
  { kCALL, { kCOND_C, ka16, }, 3 }, { kInvalid, { kNONE, kNONE }, 1 },
  { kSBC, { kA, kd8 }, 2 }, { kRST, { kLITERAL_0x18, kNONE }, 1 },
  // Ex
  { kLD, { kDEREF_a8, kA }, 2 }, { kPOP, { kHL, kNONE }, 1 },
  { kLD, { kDEREF_C, kA }, 1 }, { kInvalid, { kNONE, kNONE }, 1 },
  { kInvalid, { kNONE, kNONE }, 1 }, { kPUSH, { kHL, kNONE }, 1 },
  { kAND, { kd8, kNONE }, 2 }, { kRST, { kLITERAL_0x20, kNONE }, 1 },

  { kADD, { kSP, kr8 }, 2 }, { kJP, { kNONE, kDEREF_HL }, 2 },
  { kLD, { kDEREF_a16, kA }, 3 }, { kInvalid, { kNONE, kNONE }, 1 },
  { kInvalid, { kNONE, kNONE }, 1 }, { kInvalid, { kNONE, kNONE }, 1 },
  { kXOR, { kd8, kNONE }, 1 }, { kRST, { kLITERAL_0x28, kNONE }, 1 },
  // Fx
  { kLD, { kA, kDEREF_a8 }, 2 }, { kPOP, { kAF, kNONE }, 1 },
  { kLD, { kA, kDEREF_C }, 1 }, { kDI, { kNONE, kNONE }, 1 },
  { kInvalid, { kNONE, kNONE }, 1 }, { kPUSH, { kAF, kNONE }, 1 },
  { kOR, { kd8, kNONE }, 2 }, { kRST, { kLITERAL_0x30, kNONE }, 1 },

  { kLD, { kHL, kSP_r8}, 2 }, { kLD, { kSP, kHL }, 1 },
  { kLD, { kA, kDEREF_a16 }, 3 }, { kEI, { kNONE, kNONE }, 1 },
  { kInvalid, { kNONE, kNONE }, 1 }, { kInvalid, { kNONE, kNONE }, 1 },
  { kCP, { kd8, kNONE }, 2 }, { kRST, { kLITERAL_0x38, kNONE }, 1 },
};

static const struct instruction cb_instruction_table [256] = {
  // 0x
  { kRLC, { kB, kNONE }, 2 }, { kRLC, { kC, kNONE }, 2 },
  { kRLC, { kD, kNONE }, 2 }, { kRLC, { kE, kNONE }, 2 },
  { kRLC, { kH, kNONE }, 2 }, { kRLC, { kL, kNONE }, 2 },
  { kRLC, { kDEREF_HL, kNONE }, 2 }, { kRLC, { kA, kNONE }, 2 },

  { kRRC, { kB, kNONE }, 2 }, { kRRC, { kC, kNONE }, 2 },
  { kRRC, { kD, kNONE }, 2 }, { kRRC, { kE, kNONE }, 2 },
  { kRRC, { kH, kNONE }, 2 }, { kRRC, { kL, kNONE }, 2 },
  { kRRC, { kDEREF_HL, kNONE }, 2 }, { kRRC, { kA, kNONE }, 2 },
  // 1x
  { kRL, { kB, kNONE }, 2 }, { kRL, { kC, kNONE }, 2 },
  { kRL, { kD, kNONE }, 2 }, { kRL, { kE, kNONE }, 2 },
  { kRL, { kH, kNONE }, 2 }, { kRL, { kL, kNONE }, 2 },
  { kRL, { kDEREF_HL, kNONE }, 2 }, { kRL, { kA, kNONE }, 2 },

  { kRR, { kB, kNONE }, 2 }, { kRR, { kC, kNONE }, 2 },
  { kRR, { kD, kNONE }, 2 }, { kRR, { kE, kNONE }, 2 },
  { kRR, { kH, kNONE }, 2 }, { kRR, { kL, kNONE }, 2 },
  { kRR, { kDEREF_HL, kNONE }, 2 }, { kRR, { kA, kNONE }, 2 },
  // 2x
  { kSLA, { kB, kNONE }, 2 }, { kSLA, { kC, kNONE }, 2 },
  { kSLA, { kD, kNONE }, 2 }, { kSLA, { kE, kNONE }, 2 },
  { kSLA, { kH, kNONE }, 2 }, { kSLA, { kL, kNONE }, 2 },
  { kSLA, { kDEREF_HL, kNONE }, 2 }, { kSLA, { kA, kNONE }, 2 },

  { kSRA, { kB, kNONE }, 2 }, { kSRA, { kC, kNONE }, 2 },
  { kSRA, { kD, kNONE }, 2 }, { kSRA, { kE, kNONE }, 2 },
  { kSRA, { kH, kNONE }, 2 }, { kSRA, { kL, kNONE }, 2 },
  { kSRA, { kDEREF_HL, kNONE }, 2 }, { kSRA, { kA, kNONE }, 2 },
  // 3x
  { kSWAP, { kB, kNONE }, 2 }, { kSWAP, { kC, kNONE }, 2 },
  { kSWAP, { kD, kNONE }, 2 }, { kSWAP, { kE, kNONE }, 2 },
  { kSWAP, { kH, kNONE }, 2 }, { kSWAP, { kL, kNONE }, 2 },
  { kSWAP, { kDEREF_HL, kNONE }, 2 }, { kSWAP, { kA, kNONE }, 2 },

  { kSRL, { kB, kNONE }, 2 }, { kSRL, { kC, kNONE }, 2 },
  { kSRL, { kD, kNONE }, 2 }, { kSRL, { kE, kNONE }, 2 },
  { kSRL, { kH, kNONE }, 2 }, { kSRL, { kL, kNONE }, 2 },
  { kSRL, { kDEREF_HL, kNONE }, 2 }, { kSRL, { kA, kNONE }, 2 },
  // 4x
  { kBIT, { kBIT_0, kB }, 2 }, { kBIT, { kBIT_0, kC }, 2 },
  { kBIT, { kBIT_0, kD }, 2 }, { kBIT, { kBIT_0, kE }, 2 },
  { kBIT, { kBIT_0, kH }, 2 }, { kBIT, { kBIT_0, kL }, 2 },
  { kBIT, { kBIT_0, kDEREF_HL }, 2 }, { kBIT, { kBIT_0, kA }, 2 },

  { kBIT, { kBIT_1, kB }, 2 }, { kBIT, { kBIT_1, kC }, 2 },
  { kBIT, { kBIT_1, kD }, 2 }, { kBIT, { kBIT_1, kE }, 2 },
  { kBIT, { kBIT_1, kH }, 2 }, { kBIT, { kBIT_1, kL }, 2 },
  { kBIT, { kBIT_1, kDEREF_HL }, 2 }, { kBIT, { kBIT_1, kA }, 2 },
  // 5x
  { kBIT, { kBIT_2, kB }, 2 }, { kBIT, { kBIT_2, kC }, 2 },
  { kBIT, { kBIT_2, kD }, 2 }, { kBIT, { kBIT_2, kE }, 2 },
  { kBIT, { kBIT_2, kH }, 2 }, { kBIT, { kBIT_2, kL }, 2 },
  { kBIT, { kBIT_2, kDEREF_HL }, 2 }, { kBIT, { kBIT_2, kA }, 2 },

  { kBIT, { kBIT_3, kB }, 2 }, { kBIT, { kBIT_3, kC }, 2 },
  { kBIT, { kBIT_3, kD }, 2 }, { kBIT, { kBIT_3, kE }, 2 },
  { kBIT, { kBIT_3, kH }, 2 }, { kBIT, { kBIT_3, kL }, 2 },
  { kBIT, { kBIT_3, kDEREF_HL }, 2 }, { kBIT, { kBIT_3, kA }, 2 },
  // 6x
  { kBIT, { kBIT_4, kB }, 2 }, { kBIT, { kBIT_4, kC }, 2 },
  { kBIT, { kBIT_4, kD }, 2 }, { kBIT, { kBIT_4, kE }, 2 },
  { kBIT, { kBIT_4, kH }, 2 }, { kBIT, { kBIT_4, kL }, 2 },
  { kBIT, { kBIT_4, kDEREF_HL }, 2 }, { kBIT, { kBIT_4, kA }, 2 },

  { kBIT, { kBIT_5, kB }, 2 }, { kBIT, { kBIT_5, kC }, 2 },
  { kBIT, { kBIT_5, kD }, 2 }, { kBIT, { kBIT_5, kE }, 2 },
  { kBIT, { kBIT_5, kH }, 2 }, { kBIT, { kBIT_5, kL }, 2 },
  { kBIT, { kBIT_5, kDEREF_HL }, 2 }, { kBIT, { kBIT_5, kA }, 2 },
  // 7x
  { kBIT, { kBIT_6, kB }, 2 }, { kBIT, { kBIT_6, kC }, 2 },
  { kBIT, { kBIT_6, kD }, 2 }, { kBIT, { kBIT_6, kE }, 2 },
  { kBIT, { kBIT_6, kH }, 2 }, { kBIT, { kBIT_6, kL }, 2 },
  { kBIT, { kBIT_6, kDEREF_HL }, 2 }, { kBIT, { kBIT_6, kA }, 2 },

  { kBIT, { kBIT_7, kB }, 2 }, { kBIT, { kBIT_7, kC }, 2 },
  { kBIT, { kBIT_7, kD }, 2 }, { kBIT, { kBIT_7, kE }, 2 },
  { kBIT, { kBIT_7, kH }, 2 }, { kBIT, { kBIT_7, kL }, 2 },
  { kBIT, { kBIT_7, kDEREF_HL }, 2 }, { kBIT, { kBIT_7, kA }, 2 },
  // 8x
  { kRES, { kBIT_0, kB }, 2 }, { kRES, { kBIT_0, kC }, 2 },
  { kRES, { kBIT_0, kD }, 2 }, { kRES, { kBIT_0, kE }, 2 },
  { kRES, { kBIT_0, kH }, 2 }, { kRES, { kBIT_0, kL }, 2 },
  { kRES, { kBIT_0, kDEREF_HL }, 2 }, { kRES, { kBIT_0, kA }, 2 },

  { kRES, { kBIT_1, kB }, 2 }, { kRES, { kBIT_1, kC }, 2 },
  { kRES, { kBIT_1, kD }, 2 }, { kRES, { kBIT_1, kE }, 2 },
  { kRES, { kBIT_1, kH }, 2 }, { kRES, { kBIT_1, kL }, 2 },
  { kRES, { kBIT_1, kDEREF_HL }, 2 }, { kRES, { kBIT_1, kA }, 2 },
  // 9x
  { kRES, { kBIT_2, kB }, 2 }, { kRES, { kBIT_2, kC }, 2 },
  { kRES, { kBIT_2, kD }, 2 }, { kRES, { kBIT_2, kE }, 2 },
  { kRES, { kBIT_2, kH }, 2 }, { kRES, { kBIT_2, kL }, 2 },
  { kRES, { kBIT_2, kDEREF_HL }, 2 }, { kRES, { kBIT_2, kA }, 2 },

  { kRES, { kBIT_3, kB }, 2 }, { kRES, { kBIT_3, kC }, 2 },
  { kRES, { kBIT_3, kD }, 2 }, { kRES, { kBIT_3, kE }, 2 },
  { kRES, { kBIT_3, kH }, 2 }, { kRES, { kBIT_3, kL }, 2 },
  { kRES, { kBIT_3, kDEREF_HL }, 2 }, { kRES, { kBIT_3, kA }, 2 },
  // Ax
  { kRES, { kBIT_4, kB }, 2 }, { kRES, { kBIT_4, kC }, 2 },
  { kRES, { kBIT_4, kD }, 2 }, { kRES, { kBIT_4, kE }, 2 },
  { kRES, { kBIT_4, kH }, 2 }, { kRES, { kBIT_4, kL }, 2 },
  { kRES, { kBIT_4, kDEREF_HL }, 2 }, { kRES, { kBIT_4, kA }, 2 },

  { kRES, { kBIT_5, kB }, 2 }, { kRES, { kBIT_5, kC }, 2 },
  { kRES, { kBIT_5, kD }, 2 }, { kRES, { kBIT_5, kE }, 2 },
  { kRES, { kBIT_5, kH }, 2 }, { kRES, { kBIT_5, kL }, 2 },
  { kRES, { kBIT_5, kDEREF_HL }, 2 }, { kRES, { kBIT_5, kA }, 2 },
  // Bx
  { kRES, { kBIT_6, kB }, 2 }, { kRES, { kBIT_6, kC }, 2 },
  { kRES, { kBIT_6, kD }, 2 }, { kRES, { kBIT_6, kE }, 2 },
  { kRES, { kBIT_6, kH }, 2 }, { kRES, { kBIT_6, kL }, 2 },
  { kRES, { kBIT_6, kDEREF_HL }, 2 }, { kRES, { kBIT_6, kA }, 2 },

  { kRES, { kBIT_7, kB }, 2 }, { kRES, { kBIT_7, kC }, 2 },
  { kRES, { kBIT_7, kD }, 2 }, { kRES, { kBIT_7, kE }, 2 },
  { kRES, { kBIT_7, kH }, 2 }, { kRES, { kBIT_7, kL }, 2 },
  { kRES, { kBIT_7, kDEREF_HL }, 2 }, { kRES, { kBIT_7, kA }, 2 },
  // Cx
  { kSET, { kBIT_0, kB }, 2 }, { kSET, { kBIT_0, kC }, 2 },
  { kSET, { kBIT_0, kD }, 2 }, { kSET, { kBIT_0, kE }, 2 },
  { kSET, { kBIT_0, kH }, 2 }, { kSET, { kBIT_0, kL }, 2 },
  { kSET, { kBIT_0, kDEREF_HL }, 2 }, { kSET, { kBIT_0, kA }, 2 },

  { kSET, { kBIT_1, kB }, 2 }, { kSET, { kBIT_1, kC }, 2 },
  { kSET, { kBIT_1, kD }, 2 }, { kSET, { kBIT_1, kE }, 2 },
  { kSET, { kBIT_1, kH }, 2 }, { kSET, { kBIT_1, kL }, 2 },
  { kSET, { kBIT_1, kDEREF_HL }, 2 }, { kSET, { kBIT_1, kA }, 2 },
  // Dx
  { kSET, { kBIT_2, kB }, 2 }, { kSET, { kBIT_2, kC }, 2 },
  { kSET, { kBIT_2, kD }, 2 }, { kSET, { kBIT_2, kE }, 2 },
  { kSET, { kBIT_2, kH }, 2 }, { kSET, { kBIT_2, kL }, 2 },
  { kSET, { kBIT_2, kDEREF_HL }, 2 }, { kSET, { kBIT_2, kA }, 2 },

  { kSET, { kBIT_3, kB }, 2 }, { kSET, { kBIT_3, kC }, 2 },
  { kSET, { kBIT_3, kD }, 2 }, { kSET, { kBIT_3, kE }, 2 },
  { kSET, { kBIT_3, kH }, 2 }, { kSET, { kBIT_3, kL }, 2 },
  { kSET, { kBIT_3, kDEREF_HL }, 2 }, { kSET, { kBIT_3, kA }, 2 },
  // Ex
  { kSET, { kBIT_4, kB }, 2 }, { kSET, { kBIT_4, kC }, 2 },
  { kSET, { kBIT_4, kD }, 2 }, { kSET, { kBIT_4, kE }, 2 },
  { kSET, { kBIT_4, kH }, 2 }, { kSET, { kBIT_4, kL }, 2 },
  { kSET, { kBIT_4, kDEREF_HL }, 2 }, { kSET, { kBIT_4, kA }, 2 },

  { kSET, { kBIT_5, kB }, 2 }, { kSET, { kBIT_5, kC }, 2 },
  { kSET, { kBIT_5, kD }, 2 }, { kSET, { kBIT_5, kE }, 2 },
  { kSET, { kBIT_5, kH }, 2 }, { kSET, { kBIT_5, kL }, 2 },
  { kSET, { kBIT_5, kDEREF_HL }, 2 }, { kSET, { kBIT_5, kA }, 2 },
  // Fx
  { kSET, { kBIT_6, kB }, 2 }, { kSET, { kBIT_6, kC }, 2 },
  { kSET, { kBIT_6, kD }, 2 }, { kSET, { kBIT_6, kE }, 2 },
  { kSET, { kBIT_6, kH }, 2 }, { kSET, { kBIT_6, kL }, 2 },
  { kSET, { kBIT_6, kDEREF_HL }, 2 }, { kSET, { kBIT_6, kA }, 2 },

  { kSET, { kBIT_7, kB }, 2 }, { kSET, { kBIT_7, kC }, 2 },
  { kSET, { kBIT_7, kD }, 2 }, { kSET, { kBIT_7, kE }, 2 },
  { kSET, { kBIT_7, kH }, 2 }, { kSET, { kBIT_7, kL }, 2 },
  { kSET, { kBIT_7, kDEREF_HL }, 2 }, { kSET, { kBIT_7, kA }, 2 },
};

void decode_instruction (const struct rom* const rom, const uint16_t pc,
    const struct instruction** const instruction) {
  const uint8_t first_byte = rom->data[pc];
  if (first_byte == 0xCB) {
    const uint8_t second_byte = rom->data[pc + 1];
    *instruction = &cb_instruction_table[second_byte];
  } else {
    *instruction = &instruction_table[first_byte];
  }
#ifndef NDEBUG
  if ((*instruction)->opcode == kInvalid) {
    fprintf(stderr, "Looks like were trying to decode data as instructions,\n"
        "rest of disassembly may be invalid from this point.\n WARN: "
        PRIshort "\n", pc);
  }
#endif
}

// TODO: maybe make this some kind of ToString() like fn?
void print_instruction (const struct instruction* const instruction) {
  const enum Opcode op = instruction->opcode;
  const enum Operand op0 = instruction->operands[0];
  const enum Operand op1 = instruction->operands[1];

// intentionally keep this if NDEBUG is used, otherwise check during decode in
// order to get pc.
#ifdef NDEBUG
  if (op == kInvalid) {
    fprintf(stderr, "Looks like were trying to decode data as instructions,\n"
        "rest of disassembly may be invalid from this point.\n");
  }
#endif

  printf("%s %s %s: %d\n",
      opcode_str_table[op],
      operand_str_table[op0],
      operand_str_table[op1],
      instruction->length);
}

int disassemble (const struct rom* const rom) {
  uint16_t pc = 0;
  const struct instruction* instruction = NULL;
  while (pc < rom->size) {
    decode_instruction(rom, pc, &instruction);
    assert(instruction->length > 0);
    assert(instruction->length < 4);
    printf(PRIshort ": ", pc);
    print_instruction(instruction);
    pc += instruction->length;
  }
  return 0;
}

static void print_sizes () {
  printf(
      "sizeof struct instruction: %lu, sizeof opcode: %lu, "
      "sizeof operand: %lu\n",
      sizeof(struct instruction), sizeof(enum Opcode), sizeof(enum Operand));
}

int main (int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: ./disassembler <rom.gb>\n");
    return -1;
  }
  print_sizes();

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
