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

#define nrp          u32*
#define nrg(addr)    (*((nrp)(addr)))
#define vrp          volatile u32*
#define vrg(addr)    (*((vrp)(addr)))

#define _Y_SIZE 0x20000
#define _CBCR_SIZE 0x10000
#define _BASE 0
#define SRC_BUFFER_Y        _BASE
#define SRC_BUFFER_CB       _BASE + _Y_SIZE
#define SRC_BUFFER_CR       SRC_BUFFER_CB + _CBCR_SIZE
#define DST_BUFFER_HEIGHT   272
#define DST_BUFFER_WIDTH    480
#define BLOCKS_HCOUNT   34 /* 34 x 16 */
#define BLOCKS_WCOUNT   30 /* 30 x 16 */

#define me_section_size (&__stop__me_section - &__start__me_section)
#define _meLoop      vrg((0xbfc00040 + me_section_size))

static inline void meDCacheWritebackInvalidAll() {
 asm("sync");
 for (int i = 0; i < 8192; i += 64) {
  asm("cache 0x14, 0(%0)" :: "r"(i));
  asm("cache 0x14, 0(%0)" :: "r"(i));
 }
 asm("sync");
}

static inline u32* meSetUserMem(const u32 size) {
  static void* _base = nullptr;
  if (!_base) {
    _base = memalign(16, size*4);
    memset(_base, 0, size);
    return (u32*)(0x40000000 | (u32)_base);
  } else if (!size) {
    free(_base);
  }
  return nullptr;
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

static volatile bool _meExit = false;
static inline void meExit() {
  _meExit = true;
  meDCacheWritebackInvalidAll();
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
