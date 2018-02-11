#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PRIbyte "0x%02hhX"
static void pbyte (const uint8_t b) {
  printf(PRIbyte "\n", b);
}
#define PRIshort "0x%04hX"
static void pshort (const uint16_t s) {
  printf(PRIshort "\n", s);
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
  kJR,
  kCALL,
  kRET,
  kRST,
  // prefix
  kCB,
  // Interrupts
  kEI,
  kDI
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
  "CB",
  "EI",
  "DI"
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
  // conditions (only used by JP)
  kCOND_Z,
  kCOND_NZ,
  kCOND_C,
  kCOND_NC,
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

struct instruction {
  // where in the rom we found this
  uint16_t rom_addr;
  // the byte values you'd see in hexdump
  /*uint8_t raw_values [3];*/
  enum Opcode opcode;
  enum Operand operands [2];
  /*enum CbOpcode cb_opcode;*/
  // length in bytes for this instruction
  uint8_t length;
  /*char* name;*/
};

void decode_instruction (const struct rom* const rom, const uint16_t pc,
    struct instruction* const instruction) {
  memset(instruction, 0, sizeof(*instruction));
  instruction->rom_addr = pc;
  const uint8_t first_byte = rom->data[pc];
  instruction->opcode = decode_table[first_byte];
  // Not sure we ever want assert here. Parsing should not fail because this
  // may be static data that we're trying to interpret as an instruction.
  /*assert(instruction->opcode != kInvalid);*/
  if (instruction->opcode == kInvalid) {
    fprintf(stderr, "Looks like were trying to decode data as instructions,\n"
        "rest of disassembly may be invalid from this point.\n WARN: "
        PRIshort "\n", pc);
  }
  instruction->operands[0] = operand_0_table[first_byte];
  instruction->operands[1] = operand_1_table[first_byte];
  // calculate instruction length
  const uint8_t len = 1 +
    (instruction->opcode == kCB) +
    (instruction->operands[0] > kINSTRUCTION_LENGTH_0) +
    (instruction->operands[0] > kINSTRUCTION_LENGTH_1) +
    (instruction->operands[1] > kINSTRUCTION_LENGTH_0) +
    (instruction->operands[1] > kINSTRUCTION_LENGTH_1);
  assert(len > 0);
  assert(len < 4);
  instruction->length = len;
}

void print_instruction (const struct instruction* const instruction) {
  const enum Operand op0 = instruction->operands[0];
  assert(op0 != kINSTRUCTION_LENGTH_0);
  assert(op0 != kINSTRUCTION_LENGTH_1);

  printf(PRIshort ": %s %s %s: %d\n",
      instruction->rom_addr,
      opcode_str_table[instruction->opcode],
      operand_str_table[instruction->operands[0]],
      operand_str_table[instruction->operands[1]],
      instruction->length);
}

int disassemble (const struct rom* const rom) {
  uint16_t pc = 0;
  struct instruction instruction;
  while (pc < rom->size) {
    decode_instruction(rom, pc, &instruction);
    print_instruction(&instruction);
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
