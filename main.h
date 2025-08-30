#pragma once
#include <psppower.h>
#include <pspdisplay.h>
#include <pspsdk.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include <cstring>
#include <malloc.h>
#include <stdio.h>
#include "kcall.h"

#define u8  unsigned char
#define u16 unsigned short int
#define u32 unsigned int

#define hwp          volatile u32*
#define hw(addr)     (*((hwp)(addr)))
#define uhw(addr)    ((u32*)(0x40000000 | ((u32)addr)))

#define ME_EDRAM_BASE         0x00000000
#define GE_EDRAM_BASE         0x04000000
#define UNCACHED_USER_MASK    0x40000000
#define ME_HANDLER_BASE       0xbfc00000
#define UNCACHED_KERNEL_MASK  0xA0000000

static inline void meDcacheWritebackInvalidateAll() {
 asm("sync");
 for (int i = 0; i < 8192; i += 64) {
  asm("cache 0x14, 0(%0)" :: "r"(i));
  asm("cache 0x14, 0(%0)" :: "r"(i));
 }
 asm("sync");
}

inline void meHalt() {
  asm volatile(".word 0x70000000");
}

inline void meGetUncached32(volatile u32** const mem, const u32 size) {
  static void* _base = nullptr;
  if (!_base) {
    const u32 byteCount = size * 4;
    _base = memalign(16, byteCount);
    memset(_base, 0, byteCount);
    sceKernelDcacheWritebackInvalidateAll();
    *mem = (u32*)(UNCACHED_USER_MASK | (u32)_base);
    __asm__ volatile (
      "cache 0x1b, 0(%0)  \n"
      "sync               \n"
      : : "r" (mem) : "memory"
    );
    return;
  } else if (!size) {
    free(_base);
  }
  *mem = nullptr;
  return;
}

static inline u8* getByteFromFile(const char* name, const u32 size) {
  FILE* const f = fopen(name, "rb");
  if (f != nullptr) {
    u8* const buffer = (u8*)memalign(16, size);
    fread((void*)buffer, sizeof(u8), size, f);
    fclose(f);
    return buffer;
  }
  return nullptr;
}

static inline u16 q37(float value) {
  if (value < -4.0f || value >= 4.0f) {
    return 0;
  }
  float temp = value * 128.0f;
  int q37v = (int)temp;
  if (q37v < 0) {
    q37v += 1024;
  }
  return (u16)(q37v & 0x03FF);
}

template<typename T>
inline T xorshift() {
  static T state = 1;
  T x = state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return state = x;
}

unsigned short int randInRange(const unsigned short int range) {
  unsigned short int x = xorshift<unsigned int>();
  unsigned int m = (unsigned int)x * (unsigned int)range;
  return (m >> 16);
}
