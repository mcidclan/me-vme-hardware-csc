## CSC Sample Code
This repository provides a sample implementation for converting YCbCr data to RGB using the Media Engine and VME.
It demonstrates how to load YCbCr planes from files on the main CPU into an uncached memory area,
transfer them to the eDRAM of the Media Engine, and then process the RGB conversion via the VME before each buffer swap.
The final output is written to VRAM, serving as the destination framebuffer.

# Requirements
Run `./build.sh` from a bash shell.
Place the `y.bin`, `cb.bin`, `cr.bin`, and `prx` files along with the `EBOOT` in a `me` folder inside your `GAME` folder before launching.

## Special Thanks To
- Contributors to psdevwiki.
- The PSP homebrew community, for being an invaluable source of knowledge.
- *crazyc* from ps2dev.org, for pioneering discoveries related to the Media Engine.
- All developers and contributors who have supported and continue to support the scene.

# resources:
- [uofw on GitHub](https://github.com/uofw/uofw)
- [psp wiki on PSDevWiki](https://www.psdevwiki.com/psp/)
- [pspdev on GitHub](https://github.com/pspdev)

*m-c/d*
