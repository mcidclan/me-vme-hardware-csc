#include "main.h"

PSP_MODULE_INFO("me-csc-vme", 0, 1, 1);
PSP_HEAP_SIZE_KB(-1024);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_VFPU | PSP_THREAD_ATTR_USER);

static volatile u32* mem = nullptr;
// Set up the me shared variables in uncached shared user memory.
#define y         (mem[0])
#define cb        (mem[1])
#define cr        (mem[2])
#define refresh   (mem[3])

// Load data from a source buffer into a YCbCr destination plane buffer,
// and optionally add a random value to each byte.
void loadBuffer(const u32 buffer, const u32 size, const u32 source, const bool useRand = false) {
  u8 v = 0;
  for (u32 i = 0; i < size; i++) {
    v = *((u8*)(source + i));
    v = (useRand && (v < 190)) ? v + randInRange(32) : v;
    *((u8*)((0xA0000000 | buffer) + i)) = v;
  }
}

__attribute__((noinline, aligned(4)))
static int meLoop() {
  // Wait until mem is ready
  while (!mem || !y) {
    meDCacheWritebackInvalidAll();
  }
  do {
    // Update buffer only when needed
    if (!refresh) {
      loadBuffer(SRC_BUFFER_Y, 480*272, y, true);
      loadBuffer(SRC_BUFFER_CB, 240*136, cb);
      loadBuffer(SRC_BUFFER_CR, 240*136, cr);
      refresh = 1;
    }
  } while(!_meExit);
  return _meExit;
}

extern char __start__me_section;
extern char __stop__me_section;
__attribute__((section("_me_section"), noinline, aligned(4)))
void meHandler() {
  vrg(0xbc100050) = 0x7f;       // enable clocks: ME, AW bus RegA, RegB & Edram, DMACPlus, DMAC
  vrg(0xbc100004) = 0xffffffff; // clear NMI
  vrg(0xbc100040) = 2;          // allow 64MB ram
  asm("sync");
  ((FCall)_meLoop)();
}

static int initMe() {
  memcpy((void *)0xbfc00040, (void*)&__start__me_section, me_section_size);
  _meLoop = (u32)&meLoop;
  meDCacheWritebackInvalidAll();
  // reset and start me
  vrg(0xBC10004C) = 0b100;
  asm("sync");
  vrg(0xBC10004C) = 0x0;
  asm("sync");
  return 0;
}

static u32 DST_BUFFER_RGB  = 0x44100000;

// swap buffers and update frame buffer destination
static void swapBuffers() {
  static u32 displayBuffer = 0x44000000;
  const u32 dst = displayBuffer;
  displayBuffer = DST_BUFFER_RGB;
  DST_BUFFER_RGB = dst;
  sceDisplaySetFrameBuf((void*)displayBuffer, 512, 3, PSP_DISPLAY_SETBUF_NEXTFRAME);
}

static int startCSC() {
  // update drawing buffer destination
  vrg(0xBC800144) = DST_BUFFER_RGB;
  asm("sync");
  // start csc rendering
  vrg(0xBC800160) = 1;
  asm("sync");
  return 0;
}

// Set up color space conversion hardware registers
static int setupCSC() {
  vrg(0xBC800120) = SRC_BUFFER_Y;
  vrg(0xBC800130) = SRC_BUFFER_CB;
  vrg(0xBC800134) = SRC_BUFFER_CR;
  
  // bit [...16] height/16 * n dest buffer | bit [...8 ] width/16 | bit[2:1] ignore dest 2/line replication control | use avc/vme
  vrg(0xBC800140) = (BLOCKS_HCOUNT << 16) | (BLOCKS_WCOUNT << 8) | 1 << 2 | 1 << 1 | 1;
  vrg(0xBC800144) = DST_BUFFER_RGB;
  vrg(0xBC800148) = DST_BUFFER_RGB; // unused/ignored, set to either 0 or any
  
  // bit [...8] stride | bit[1] ycbcr format | bit[0] separate dst 2 rendering
  vrg(0xBC80014C) = (512 << 8) | 0 << 1 | 1;
  
  // Use default matrice values, with adjusted luma
  const float brightness = 1.16f;
  vrg(0xBC800150) = 0x0CC << 20 | 0x000 << 10 | q37(brightness); // r
  vrg(0xBC800154) = 0x398 << 20 | 0x3CE << 10 | q37(brightness); // g
  vrg(0xBC800158) = 0x000 << 20 | 0x102 << 10 | q37(brightness); // b
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
  mem = meSetUserMem(4);

  // Load Y, Cb and Cr
  u8* const _y = getByteFromFile("y.bin", 480*272);
  u8* const _cb = getByteFromFile("cb.bin", 240*136);
  u8* const _cr = getByteFromFile("cr.bin", 240*136);
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
  } while(!(ctl.Buttons & PSP_CTRL_HOME));
  
  // exit me
  meExit();
  
  // clean y,cb,cr planes
  free(_y);
  free(_cb);
  free(_cr);
  
  // clean allocated me user memory
  meSetUserMem(0);
  
  // exit
  pspDebugScreenInit();
  pspDebugScreenClear();
  pspDebugScreenSetXY(0, 1);
  pspDebugScreenPrintf("Exiting...");
  sceKernelDelayThread(500000);
  sceKernelExitGame();
  return 0;
}
