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

static const enum Opcode decode_table [256] = {
  // 0x
  kNOP, kLD, kLD, kINC, kINC, kDEC, kLD, kRLC,
  kLD, kADD, kLD, kDEC, kINC, kDEC, kLD, kRRC,
  // 1x
  kSTOP, kLD, kLD, kINC, kINC, kDEC, kLD, kRL,
  kJR, kADD, kLD, kDEC, kINC, kDEC, kLD, kRR,
  // 2x
  kJR, kLD, kLD, kINC, kINC, kDEC, kLD, kDAA,
  kJR, kADD, kLD, kDEC, kINC, kDEC, kLD, kCPL,
  // 3x
  kJR, kLD, kLD, kINC, kINC, kDEC, kLD, kSCF,
  kJR, kADD, kLD, kDEC, kINC, kDEC, kLD, kCCF,
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

static const enum Opcode cb_decode_table [256] = {
  // 0x
  kRLC, kRLC, kRLC, kRLC, kRLC, kRLC, kRLC, kRLC,
  kRRC, kRRC, kRRC, kRRC, kRRC, kRRC, kRRC, kRRC,
  // 1x
  kRL, kRL, kRL, kRL, kRL, kRL, kRL, kRL,
  kRR, kRR, kRR, kRR, kRR, kRR, kRR, kRR,
  // 2x
  kSLA, kSLA, kSLA, kSLA, kSLA, kSLA, kSLA, kSLA,
  kSRA, kSRA, kSRA, kSRA, kSRA, kSRA, kSRA, kSRA,
  // 3x
  kSWAP, kSWAP, kSWAP, kSWAP, kSWAP, kSWAP, kSWAP, kSWAP,
  kSRL, kSRL, kSRL, kSRL, kSRL, kSRL, kSRL, kSRL,
  // 4x
  kBIT, kBIT, kBIT, kBIT, kBIT, kBIT, kBIT, kBIT,
  kBIT, kBIT, kBIT, kBIT, kBIT, kBIT, kBIT, kBIT,
  // 5x
  kBIT, kBIT, kBIT, kBIT, kBIT, kBIT, kBIT, kBIT,
  kBIT, kBIT, kBIT, kBIT, kBIT, kBIT, kBIT, kBIT,
  // 6x
  kBIT, kBIT, kBIT, kBIT, kBIT, kBIT, kBIT, kBIT,
  kBIT, kBIT, kBIT, kBIT, kBIT, kBIT, kBIT, kBIT,
  // 7x
  kBIT, kBIT, kBIT, kBIT, kBIT, kBIT, kBIT, kBIT,
  kBIT, kBIT, kBIT, kBIT, kBIT, kBIT, kBIT, kBIT,
  // 8x
  kRES, kRES, kRES, kRES, kRES, kRES, kRES, kRES,
  kRES, kRES, kRES, kRES, kRES, kRES, kRES, kRES,
  // 9x
  kRES, kRES, kRES, kRES, kRES, kRES, kRES, kRES,
  kRES, kRES, kRES, kRES, kRES, kRES, kRES, kRES,
  // Ax
  kRES, kRES, kRES, kRES, kRES, kRES, kRES, kRES,
  kRES, kRES, kRES, kRES, kRES, kRES, kRES, kRES,
  // Bx
  kRES, kRES, kRES, kRES, kRES, kRES, kRES, kRES,
  kRES, kRES, kRES, kRES, kRES, kRES, kRES, kRES,
  // Cx
  kSET, kSET, kSET, kSET, kSET, kSET, kSET, kSET,
  kSET, kSET, kSET, kSET, kSET, kSET, kSET, kSET,
  // Dx
  kSET, kSET, kSET, kSET, kSET, kSET, kSET, kSET,
  kSET, kSET, kSET, kSET, kSET, kSET, kSET, kSET,
  // Ex
  kSET, kSET, kSET, kSET, kSET, kSET, kSET, kSET,
  kSET, kSET, kSET, kSET, kSET, kSET, kSET, kSET,
  // Fx
  kSET, kSET, kSET, kSET, kSET, kSET, kSET, kSET,
  kSET, kSET, kSET, kSET, kSET, kSET, kSET, kSET
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
  // Should not be assigned, just used to delimit operands that add to
  // instruction length.  Operands that appear after this enum add one to the
  // instruction length.
  kINSTRUCTION_LENGTH_0,
  // Literals
  kd8,
  kr8,
  kSP_r8,
  kDEREF_a8,
  // Should not be assigned, just used to delimit operands that add to
  // instruction length.  Operands that appear after this enum add two to the
  // instruction length.
  kINSTRUCTION_LENGTH_1,
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
  // kINSTRUCTION_LENGTH_0
  "SHOULD NOT BE POSSIBLE",
  "d8",
  "r8",
  "SP+r8",
  "(a8)",
  // kINSTRUCTION_LENGTH_1
  "SHOULD NOT BE POSSIBLE",
  "d16",
  "a16",
  "(a16)"
};

static const enum Operand operand_0_table [256] = {
  // 0x
  kNONE, kBC, kDEREF_BC, kBC, kB, kB, kB, kNONE,
  kDEREF_a16, kHL, kA, kBC, kC, kC, kC, kNONE,
  // 1x
  kNONE, kDE, kDEREF_DE, kDE, kD, kD, kD, kNONE,
  kNONE, kHL, kA, kDE, kE, kE, kE, kNONE,
  // 2x
  kCOND_NZ, kHL, kDEREF_HL_INC, kHL, kH, kH, kH, kNONE,
  kCOND_Z, kHL, kA, kHL, kL, kL, kL, kNONE,
  // 3x
  kCOND_NC, kSP, kDEREF_HL_DEC, kSP, kDEREF_HL, kDEREF_HL, kDEREF_HL, kNONE,
  kCOND_C, kHL, kA, kSP, kA, kA, kA, kNONE,
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
  kCOND_NZ, kBC, kCOND_NZ, kNONE, kCOND_NZ, kBC, kA, kNONE,
  kNONE, kNONE, kCOND_Z, kNONE, kCOND_Z, kNONE, kA, kNONE,
  // Dx
  kCOND_NC, kDE, kCOND_NC, kNONE, kCOND_NZ, kDE, kNONE, kNONE,
  kNONE, kNONE, kCOND_C, kNONE, kNONE, kNONE, kA, kNONE,
  // Ex
  kDEREF_a8, kHL, kDEREF_C, kNONE, kNONE, kHL, kd8, kNONE,
  kSP, kDEREF_HL, kDEREF_a16, kNONE, kNONE, kNONE, kd8, kNONE,
  // Fx
  kA, kAF, kA, kNONE, kNONE, kAF, kd8, kNONE,
  kHL, kSP, kA, kNONE, kNONE, kNONE, kd8, kNONE
};

static const enum Operand operand_1_table [256] = {
  // 0x
  kNONE, kd16, kA, kNONE, kNONE, kNONE, kd8, kNONE,
  kSP, kBC, kDEREF_BC, kNONE, kNONE, kNONE, kd8, kNONE,
  // 1x
  kNONE, kd16, kA, kNONE, kNONE, kNONE, kd8, kNONE,
  kr8, kDE, kDEREF_DE, kNONE, kNONE, kNONE, kd8, kNONE,
  // 2x
  kr8, kd16, kA, kNONE, kNONE, kNONE, kd8, kNONE,
  kr8, kHL, kDEREF_HL_INC, kNONE, kNONE, kNONE, kd8, kNONE,
  // 3x
  kr8, kd16, kA, kNONE, kNONE, kNONE, kd8, kNONE,
  kr8, kSP, kDEREF_HL_DEC, kNONE, kNONE, kNONE, kd8, kNONE,
  // 4x
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // 5x
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // 6x
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // 7x
  kB, kC, kD, kE, kH, kL, kNONE, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // 8x
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // 9x
  kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // Ax
  kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE,
  kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE,
  // Bx
  kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE,
  kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE,
  // Cx
  kNONE, kNONE, ka16, ka16, ka16, kNONE, kd8, kNONE,
  kNONE, kNONE, ka16, kNONE, ka16, ka16, kd8, kNONE,
  // Dx
  kNONE, kNONE, ka16, kNONE, ka16, kNONE, kNONE, kNONE,
  kNONE, kNONE, ka16, kNONE, ka16, kNONE, kd8, kNONE,
  // Ex
  kA, kNONE, kA, kNONE, kNONE, kNONE, kNONE, kNONE,
  kr8, kDEREF_HL, kA, kNONE, kNONE, kNONE, kNONE, kNONE,
  // Fx
  kDEREF_a8, kNONE, kA, kNONE, kNONE, kNONE, kNONE, kNONE,
  kSP_r8, kHL, kDEREF_a16, kNONE, kNONE, kNONE, kNONE, kNONE
};

static const enum Operand cb_operand_0_table [256] = {
  // 0x
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // 1x
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // 2x
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // 3x
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // 4x
  kBIT_0, kBIT_0, kBIT_0, kBIT_0, kBIT_0, kBIT_0, kBIT_0, kBIT_0,
  kBIT_1, kBIT_1, kBIT_1, kBIT_1, kBIT_1, kBIT_1, kBIT_1, kBIT_1,
  // 5x
  kBIT_2, kBIT_2, kBIT_2, kBIT_2, kBIT_2, kBIT_2, kBIT_2, kBIT_2,
  kBIT_3, kBIT_3, kBIT_3, kBIT_3, kBIT_3, kBIT_3, kBIT_3, kBIT_3,
  // 6x
  kBIT_4, kBIT_4, kBIT_4, kBIT_4, kBIT_4, kBIT_4, kBIT_4, kBIT_4,
  kBIT_5, kBIT_5, kBIT_5, kBIT_5, kBIT_5, kBIT_5, kBIT_5, kBIT_5,
  // 7x
  kBIT_6, kBIT_6, kBIT_6, kBIT_6, kBIT_6, kBIT_6, kBIT_6, kBIT_6,
  kBIT_7, kBIT_7, kBIT_7, kBIT_7, kBIT_7, kBIT_7, kBIT_7, kBIT_7,
  // 8x
  kBIT_0, kBIT_0, kBIT_0, kBIT_0, kBIT_0, kBIT_0, kBIT_0, kBIT_0,
  kBIT_1, kBIT_1, kBIT_1, kBIT_1, kBIT_1, kBIT_1, kBIT_1, kBIT_1,
  // 9x
  kBIT_2, kBIT_2, kBIT_2, kBIT_2, kBIT_2, kBIT_2, kBIT_2, kBIT_2,
  kBIT_3, kBIT_3, kBIT_3, kBIT_3, kBIT_3, kBIT_3, kBIT_3, kBIT_3,
  // Ax
  kBIT_4, kBIT_4, kBIT_4, kBIT_4, kBIT_4, kBIT_4, kBIT_4, kBIT_4,
  kBIT_5, kBIT_5, kBIT_5, kBIT_5, kBIT_5, kBIT_5, kBIT_5, kBIT_5,
  // Bx
  kBIT_6, kBIT_6, kBIT_6, kBIT_6, kBIT_6, kBIT_6, kBIT_6, kBIT_6,
  kBIT_7, kBIT_7, kBIT_7, kBIT_7, kBIT_7, kBIT_7, kBIT_7, kBIT_7,
  // Cx
  kBIT_0, kBIT_0, kBIT_0, kBIT_0, kBIT_0, kBIT_0, kBIT_0, kBIT_0,
  kBIT_1, kBIT_1, kBIT_1, kBIT_1, kBIT_1, kBIT_1, kBIT_1, kBIT_1,
  // Dx
  kBIT_2, kBIT_2, kBIT_2, kBIT_2, kBIT_2, kBIT_2, kBIT_2, kBIT_2,
  kBIT_3, kBIT_3, kBIT_3, kBIT_3, kBIT_3, kBIT_3, kBIT_3, kBIT_3,
  // Ex
  kBIT_4, kBIT_4, kBIT_4, kBIT_4, kBIT_4, kBIT_4, kBIT_4, kBIT_4,
  kBIT_5, kBIT_5, kBIT_5, kBIT_5, kBIT_5, kBIT_5, kBIT_5, kBIT_5,
  // Fx
  kBIT_6, kBIT_6, kBIT_6, kBIT_6, kBIT_6, kBIT_6, kBIT_6, kBIT_6,
  kBIT_7, kBIT_7, kBIT_7, kBIT_7, kBIT_7, kBIT_7, kBIT_7, kBIT_7
};

static const enum Operand cb_operand_1_table [256] = {
  // 0x
  kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE,
  kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE,
  // 1x
  kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE,
  kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE,
  // 2x
  kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE,
  kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE,
  // 3x
  kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE,
  kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE, kNONE,
  // 4x
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // 5x
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // 6x
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // 7x
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // 8x
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // 9x
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // Ax
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // Bx
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // Cx
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // Dx
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // Ex
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  // Fx
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA,
  kB, kC, kD, kE, kH, kL, kDEREF_HL, kA
};

struct instruction {
  enum Opcode opcode;
  enum Operand operands [2];
  // length in bytes for this instruction
  uint8_t length;
};

// the pc should be pointing at prefix (0xCB) + 1
void decode_cb (const struct rom* const rom, const uint16_t pc,
    struct instruction* const instruction) {
  const uint8_t first_byte = rom->data[pc];
  instruction->opcode = cb_decode_table[first_byte];
  instruction->operands[0] = cb_operand_0_table[first_byte];
  instruction->operands[1] = cb_operand_1_table[first_byte];
  instruction->length = 2;
}

void decode_instruction (const struct rom* const rom, const uint16_t pc,
    struct instruction* const instruction) {
  memset(instruction, 0, sizeof(*instruction));
  const uint8_t first_byte = rom->data[pc];
  instruction->opcode = decode_table[first_byte];
#ifndef NDEBUG
  if (instruction->opcode == kInvalid) {
    fprintf(stderr, "Looks like were trying to decode data as instructions,\n"
        "rest of disassembly may be invalid from this point.\n WARN: "
        PRIshort "\n", pc);
    instruction->length = 1;
  } else
#endif
    if (instruction->opcode == kCB) {
    decode_cb(rom, pc + 1, instruction);
  } else {
    instruction->operands[0] = operand_0_table[first_byte];
    instruction->operands[1] = operand_1_table[first_byte];
    // calculate instruction length
    const uint8_t len = 1 +
      (instruction->operands[0] > kINSTRUCTION_LENGTH_0) +
      (instruction->operands[0] > kINSTRUCTION_LENGTH_1) +
      (instruction->operands[1] > kINSTRUCTION_LENGTH_0) +
      (instruction->operands[1] > kINSTRUCTION_LENGTH_1);
    instruction->length = len;
  }
}

// TODO: maybe make this some kind of ToString() like fn?
void print_instruction (const struct instruction* const instruction) {
  const enum Opcode op = instruction->opcode;
  const enum Operand op0 = instruction->operands[0];
  const enum Operand op1 = instruction->operands[1];
  assert(op0 != kINSTRUCTION_LENGTH_0);
  assert(op0 != kINSTRUCTION_LENGTH_1);
  assert(op1 != kINSTRUCTION_LENGTH_0);
  assert(op1 != kINSTRUCTION_LENGTH_1);

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
  struct instruction instruction;
  while (pc < rom->size) {
    decode_instruction(rom, pc, &instruction);
    printf(PRIshort ": ", pc);
    print_instruction(&instruction);
    assert(instruction.length > 0);
    assert(instruction.length < 4);
    pc += instruction.length;
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
