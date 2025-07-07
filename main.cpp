#include "main.h"

PSP_MODULE_INFO("me-csc-vme", 0, 1, 1);
PSP_HEAP_SIZE_KB(-1024);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_USER);

static u32 DST_BUFFER_RGB = GE_EDRAM_BASE | UNCACHED_USER_MASK | 0x100000;

#define STRIDE                512
#define FRAME_YCBCR_WIDTH     480
#define FRAME_YCBCR_HEIGHT    272
constexpr u32 BUFFER_2_OFFSET   = STRIDE * FRAME_YCBCR_HEIGHT * 2;
constexpr u32 FRAME_YCBCR_SIZE  = FRAME_YCBCR_WIDTH * FRAME_YCBCR_HEIGHT;
constexpr u32 Y_SIZE            = FRAME_YCBCR_SIZE;
constexpr u32 CBCR_SIZE         = FRAME_YCBCR_SIZE / 4;
constexpr u32 BLOCKS_WCOUNT     = FRAME_YCBCR_WIDTH / 16;
constexpr u32 BLOCKS_HCOUNT     = FRAME_YCBCR_HEIGHT / 8;

#define ME_BUFFER_Y             ME_EDRAM_BASE
constexpr u32 ME_BUFFER_CB      = ME_EDRAM_BASE + Y_SIZE;
constexpr u32 ME_BUFFER_CR      = ME_BUFFER_CB + CBCR_SIZE;

static volatile u32* mem = nullptr;
// Set up the me shared variables in uncached shared user memory.
#define y         (mem[0])
#define cb        (mem[1])
#define cr        (mem[2])
#define refresh   (mem[3])
#define meExit    (mem[4])

// Load data from a source buffer into a YCbCr destination plane buffer,
// and optionally add a random value to each byte.
void loadBuffer(const u32 buffer, const u32 size, const u32 source, const bool useRand = false) {
  u8 v = 0;
  for (u32 i = 0; i < size; i++) {
    v = *((u8*)(source + i));
    v = (useRand && (v < 190)) ? v + randInRange(32) : v;
    *((u8*)((UNCACHED_KERNEL_MASK | buffer) + i)) = v;
  }
}

__attribute__((noinline, aligned(4)))
static void meLoop() {
  // Wait until mem is ready
  while (!mem || !y) {
    meDcacheWritebackInvalidateAll();
  }
  do {
    // Update buffer only when needed
    if (!refresh) {
      loadBuffer(ME_BUFFER_Y, Y_SIZE, y, true);
      loadBuffer(ME_BUFFER_CB, CBCR_SIZE, cb);
      loadBuffer(ME_BUFFER_CR, CBCR_SIZE, cr);
      refresh = 1;
    }
  } while(meExit == 0);
  meExit = 2;
  meHalt();
}

extern char __start__me_section;
extern char __stop__me_section;
__attribute__((section("_me_section")))
void meHandler() {
  hw(0xbc100050) = 0x7f;        // enable buses clocks
  hw(0xbc100004) = 0xffffffff;  // enable NMIs
  hw(0xbc100040) = 0x02;        // allow 64MB ram
  asm("sync");
  
  asm volatile(
    "li          $k0, 0x30000000\n"
    "mtc0        $k0, $12\n"
    "sync\n"
    "la          $k0, %0\n"
    "li          $k1, 0x80000000\n"
    "or          $k0, $k0, $k1\n"
    "jr          $k0\n"
    "nop\n"
    :
    : "i" (meLoop)
    : "k0"
  );
}

static int initMe() {
  #define me_section_size (&__stop__me_section - &__start__me_section)
  memcpy((void *)ME_HANDLER_BASE, (void*)&__start__me_section, me_section_size);
  sceKernelDcacheWritebackInvalidateAll();
  hw(0xbc10004c) = 0x04;
  hw(0xbc10004c) = 0x0;
  asm volatile("sync");
  return 0;
}

// swap buffers and update frame buffer destination
static void swapBuffers() {
  static u32 displayBuffer = GE_EDRAM_BASE | UNCACHED_USER_MASK;
  const u32 dst = displayBuffer;
  displayBuffer = DST_BUFFER_RGB;
  DST_BUFFER_RGB = dst;
  sceDisplaySetFrameBuf((void*)displayBuffer, STRIDE, 3, PSP_DISPLAY_SETBUF_NEXTFRAME);
}

static int startCSC() {
  // update drawing buffer destination & start csc rendering
  hw(0xBC800144) = DST_BUFFER_RGB;
  asm("sync");
  hw(0xBC800160) = 1;
  asm("sync");
  return 0;
}

// Set up color space conversion hardware registers
static int setupCSC() {
  hw(0xBC800120) = ME_BUFFER_Y;
  hw(0xBC800130) = ME_BUFFER_CB;
  hw(0xBC800134) = ME_BUFFER_CR;
  
  // bit [...16] height/16 * n dest buffer | bit [...8 ] width/16 | bit[2:1] ignore dest 2/line replication control | use avc/vme
  hw(0xBC800140) = (BLOCKS_HCOUNT << 16) | (BLOCKS_WCOUNT << 8) | 1 << 2 | 1 << 1 | 1;
  hw(0xBC800144) = DST_BUFFER_RGB;
  hw(0xBC800148) = DST_BUFFER_RGB; // unused/ignored, set to either 0 or any
  
  // bit [...8] stride | bit[1] pixel format | bit[0] separate dst 2 rendering
  hw(0xBC80014C) = (STRIDE << 8) | 0 << 1 | 1;
  
  // Use default matrice values, with adjusted luma
  const float brightness = 1.16f;
  hw(0xBC800150) = 0x0CC << 20 | 0x000 << 10 | q37(brightness); // r
  hw(0xBC800154) = 0x398 << 20 | 0x3CE << 10 | q37(brightness); // g
  hw(0xBC800158) = 0x000 << 20 | 0x102 << 10 | q37(brightness); // b
  asm("sync");
  
  return 0;
}

int main() {
  scePowerSetClockFrequency(333, 333, 166);
  if (pspSdkLoadStartModule("ms0:/PSP/GAME/me/kcall.prx", PSP_MEMORY_PARTITION_KERNEL) < 0){
    sceKernelExitGame();
    return 0;
  }

  // Init me before user mem initialisation
  kcall(&initMe);
  meGetUncached32(&mem, 5);

  // Load Y, Cb and Cr
  u8* const _y = getByteFromFile("y.bin", Y_SIZE);
  u8* const _cb = getByteFromFile("cb.bin", CBCR_SIZE);
  u8* const _cr = getByteFromFile("cr.bin", CBCR_SIZE);
  if (!_y || !_cb || !_cr) {
    sceKernelExitGame();
  }
  
  // Ensure that Y, Cb, and Cr are available in the cache before performing uncached accesses
  sceKernelDcacheWritebackInvalidateAll();
  y = 0x40000000 | (u32)_y;
  cb = 0x40000000 | (u32)_cb;
  cr = 0x40000000 | (u32)_cr;
  
  // Setup csc hardware registers
  kcall(&setupCSC);
  
  SceCtrlData ctl;
  do {
    sceCtrlPeekBufferPositive(&ctl, 1);
    sceDisplayWaitVblankStart();
    if (refresh) { 
      kcall(&startCSC);
      swapBuffers();
      refresh = 0;
    }
  } while (!(ctl.Buttons & PSP_CTRL_HOME));
  
  // exit me
  meExit = 1;
  do {
    asm volatile("sync");
  } while (meExit < 2);
  
  // clean y,cb,cr planes
  free(_y);
  free(_cb);
  free(_cr);
  
  // clean allocated me user memory
  meGetUncached32(&mem, 0);
  
  // exit
  pspDebugScreenInit();
  pspDebugScreenClear();
  pspDebugScreenSetXY(0, 1);
  pspDebugScreenPrintf("Exiting...");
  sceKernelDelayThread(500000);
  sceKernelExitGame();
  return 0;
}
