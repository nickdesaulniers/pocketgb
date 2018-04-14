#include "timer.h"

#include <assert.h>

#include "logging.h"

void init_timer(struct timer* const timer, const struct mmu* const mmu) {
  assert(timer != NULL);
  assert(mmu != NULL);
  timer->mmu = (struct mmu*)mmu;
  timer->count = 0;
}

static int timer_enabled(const struct timer* const timer) {
  return rb(timer->mmu, 0xFF07) & 0x04;
}
static uint8_t get_clock_freq(const struct timer* const timer) {
  return rb(timer->mmu, 0xFF07) & 0x03;
}
/*static void update_div(struct timer* const timer) {*/
    /*++timer->mmu->memory[0xFF04];*/
/*}*/
static void update_tima(const struct timer* const timer) {
  const uint8_t tima = rb(timer->mmu, 0xFF05);
  if (tima == 255) {
    const uint8_t tma = rb(timer->mmu, 0xFF06);
    wb(timer->mmu, 0xFF05, tma);
    wb(timer->mmu, 0xFF0F, rb(timer->mmu, 0xFF0F) | 0x04);
  } else {
    wb(timer->mmu, 0xFF05, tima + 1);
  }
}

// Gameboy clock speed is 4194304 Hz (T CLK) so 1s == 4194304 cycles.
// 1 frame == 60fps == 69905 cycles per frame.
// 0xFF04 DIV
///       16384 Hz == 1/256
// 0xFF05 TIMA
// 0xFF06 TMA
// 0xFF07 TMC/TAC
///   X   enable
///   0   stop
///   1   start
///    XX clock select: 4 * 4^freq
///    00   4096 Hz == 1/1024 T CLK
///    01 262144 Hz == 1/16   T CLK
///    10  65536 Hz == 1/64   T CLK
///    11  16384 Hz == 1/256  T CLK
static const uint16_t thresholds [] = { 1024, 16, 64, 256 };
void timer_tick(struct timer* const timer, const uint8_t cycles) {
  assert(cycles % 4 == 0);
  assert(cycles >= 4);
  assert(cycles <= 24);

  // TODO: implement DIV clk
  if (timer_enabled(timer)) {
    const uint16_t freq = get_clock_freq(timer);
    const uint8_t threshold = thresholds[freq];
    timer->count += cycles;
    while (timer->count >= threshold) {
      timer->count -= threshold;
      update_tima(timer);
    }
  }
}
