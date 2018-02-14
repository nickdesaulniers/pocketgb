#pragma once

#ifndef LOG_LEVEL
#define LOG_LEVEL 0
#endif

#define LOG(LEVEL, ...) do {\
  if (LEVEL <= LOG_LEVEL) \
    printf(__VA_ARGS__); \
} while(0)

#define PRIbyte "0x%02hhX"
#define PRIshort "0x%04hX"
#define PBYTE(LEVEL, ...) LOG(LEVEL, PRIbyte "\n", __VA_ARGS__)
#define PSHORT(LEVEL, ...) LOG(LEVEL, PRIshort "\n", __VA_ARGS__)
