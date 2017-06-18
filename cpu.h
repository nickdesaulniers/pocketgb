#include <stdint.h>

struct cpu {
  struct registers {
    uint8_t a;
    struct flags {
      uint8_t z : 1;
      uint8_t n : 1;
      uint8_t h : 1;
      uint8_t c : 1;
    } f;
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
    uint16_t sp;
    uint16_t pc;
  } registers;
  uint8_t* memory;
};

typedef void (*instr) (struct cpu* const);

instr decode (const struct cpu* const);

// print 8b register
#define PR8(reg) pbyte(lr35902.registers.reg)
