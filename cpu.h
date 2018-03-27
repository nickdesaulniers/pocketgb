#pragma once

#include <stdint.h>

#include "mmu.h"

struct cpu {
  struct registers {
    union {
      uint16_t af;
      struct {
        struct flags {
          // blargg's tests crc routines require this order to be correct
          // http://gbdev.gg8.se/wiki/articles/CPU_Registers_and_Flags
          uint8_t __unused : 4;
          uint8_t c : 1;
          uint8_t h : 1;
          uint8_t n : 1;
          uint8_t z : 1;
        } f;
        uint8_t a;
      };
    };
    union {
      uint16_t bc;
      struct {
        uint8_t c;
        uint8_t b;
      };
    };
    union {
      uint16_t de;
      struct {
        uint8_t e;
        uint8_t d;
      };
    };
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
  struct mmu* mmu;
  uint8_t tick_cycles;
  uint8_t interrupts_enabled;
};

typedef void (*instr) (struct cpu* const);

uint8_t tick_once (struct cpu* const cpu);
void init_cpu (struct cpu* const restrict cpu,
    struct mmu* const restrict mmu);
