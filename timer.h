#pragma once

#include <stdint.h>

#include "mmu.h"

struct timer {
  struct mmu* mmu;
  uint16_t count;
};
void init_timer(struct timer* const timer, const struct mmu* const mmu);
void timer_tick(struct timer* const timer, const uint8_t cycles);
