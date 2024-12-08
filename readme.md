## CSC Sample Code
This sample code demonstrates how to fill three YCbCr buffer planes from the Media Engine and then let the VME process the RGB conversion.

# Overview
The goal is to populate three buffers within the Media Engine eDRAM, which are used as the source for the conversion.
The converted output is written to the base of the VRAM, which serves as the destination framebuffer.

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
