#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct cpu {
  struct registers {
    uint8_t a;
    struct flags {
      uint8_t z : 1;
      uint8_t n : 1;
      uint8_t h : 1;
      uint8_t c : 1;
    } f;
    /*uint8_t f;*/
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t e;
    union {
      uint16_t hl;
      struct {
        // order is important
        uint8_t l;
        uint8_t h;
      };
    };
    /*uint8_t h;*/
    /*uint8_t l;*/
    uint16_t sp;
    uint16_t pc;
  } registers;
};

// returns 0 on success
// TODO: fseek and compare fread size
int read_rom_into_memory (const char* const path, uint8_t* const memory) {
  FILE* f = fopen(path, "r");
  if (f) {
     fread(memory, 1, 256, f);
     fclose(f);
     return 0;
  }
  return -1;
}

uint16_t bswap (const uint16_t x) {
  return (x << 8) | (x >> 8);
}

void pbyte(const uint8_t b) {
  printf("0x%02hhX\n", b);
}

void pshort(const uint16_t s) {
  printf("0x%02hX\n", s);
}

void handle_cb_prefix (struct cpu* const lr35902, uint8_t* const memory, uint8_t* pc) {
  /*printf("CB ");*/
  switch (*pc) {
    case 0x7C:
      printf("BIT 7, H\n");
      printf("h: %d\n", lr35902->registers.h);
      printf("z: %d\n", lr35902->registers.f.z);
      lr35902->registers.f.z = (lr35902->registers.h & (1 << 6)) != 0;
      printf("z: %d\n", lr35902->registers.f.z);
      lr35902->registers.f.n = 0;
      lr35902->registers.f.h = 0;
      break;
    default:
      printf("unknown\n");
      break;
  }
}

int main (int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "USAGE: ./pocketgb <rom.gb>\n");
    return -1;
  }

  /*printf("%lu\n", sizeof(struct registers));*/

  // TODO: malloc
  uint8_t memory [65536];
  // poison value
  memset(&memory, 0xF7, 65536);
  // TODO: registers get initialized differently based on model
  struct cpu lr35902 = {0};

  int rc = read_rom_into_memory(argv[1], &memory[0]);
  if (rc != 0) {
    perror(argv[1]);
    return -1;
  }

  // http://pastraiser.com/cpu/gameboy/gameboy_opcodes.html
  for (uint8_t* pc = &memory[0]; pc < &memory[65536];) {
    /*pbyte(*pc);*/
    switch (*pc) {
      case 0x20:
        // Jump Relative
        // relative to the end of the full instruction
        printf("JR NZ,r8\n");
        printf("where r8 == %d\n", (signed char) *(pc + 1));
        if (lr35902.registers.f.z) {
          printf("not jumping\n");
          pc += 2;
          /*pc += (signed char) *(pc + 1);*/
        } else {
          printf("jumping\n");
          pc += ((signed char) *(pc + 1)) + 2;
        }
        // minus two because the instruction length of 0x20 is 2,
        // and we haven't modified the pc yet.
        /*pc += lr35902.registers.f.z ? 2 : ((signed char) *(pc + 1)) + 2;*/
        break;
      case 0x21:
        printf("LD HL,d16\n");
        pshort(lr35902.registers.hl);
        lr35902.registers.hl = *(uint16_t*)(pc + 1);
        pshort(lr35902.registers.hl);
        pc += 3;
        break;
      case 0x31:
        printf("LD SP,d16\n");
        pshort(lr35902.registers.sp);
        lr35902.registers.sp = *(uint16_t*)(pc + 1);
        pshort(lr35902.registers.sp);
        pc += 3;
        break;
      case 0x32:
        printf("LD (HL-),A\n");
        /*pshort(lr35902.registers.hl);*/
        pbyte(memory[lr35902.registers.hl]);
        memory[lr35902.registers.hl] = lr35902.registers.a;
        pbyte(memory[lr35902.registers.hl]);
        pshort(lr35902.registers.hl);
        --lr35902.registers.hl;
        pshort(lr35902.registers.hl);
        ++pc;
        break;
      case 0xAF:
        printf("XOR A\n");
        lr35902.registers.a = 0;
        ++pc;
        break;
      // these use length-2 instructions always
      case 0xCB:
        handle_cb_prefix(&lr35902, memory, ++pc);
        ++pc;
        break;
      default:
        printf("unknown instruction ");
        pbyte(*pc);
        ++pc;
        break;
    }
    printf("===\n");
  }
}
