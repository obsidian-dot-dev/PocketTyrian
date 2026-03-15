# OpenTyrian Pocket

A bare-metal, native port of **OpenTyrian** for the [Analogue Pocket](https://www.analogue.co/pocket), 
running directly on a VexRiscv RISC-V soft-core CPU instantiated on the Pocket's Cyclone V FPGA.

The core is a fork of [PocketDoom](https://github.com/thinkelastic/PocketDoom). See `README_POCKETDOOM.md`
for details on the underlying core. 

## Features

*   **Pixel-Perfect Video:** Tear-free, double-buffered rendering at the original 320x200 resolution
*   **FM Synthesis:** Uses a hardware OPL2 block (via `jotego/jtopl2`) for AdLib/SoundBlaster music synthesis.
*   **High-Fidelity Effects:** Digital PCM sound effects are processed through a custom 64-bit linear interpolator with a 4-pass FIR anti-aliasing filter, upsampled to 48kHz interleaved stereo. We even added 6dB of software headroom to eliminate the nasty clipping found in the original DOS game!
*   **Console-Optimized UI:** Tweaked UI to avoid Keyboard input and unnecessary options.
*   **Save Support:** Persistent config, and storage for 10 saves.
*   **Gamepad Support:** All inputs are mapped natively to the Pocket's face and shoulder buttons.

## Controls

| Button | Action |
| :--- | :--- |
| **D-Pad** | Move Ship / Navigate Menus |
| **A / B** | Sidekick Left / Right Weapons |
| **X** | Primary Fire |
| **Y** | Change Weapon Mode (if available) |
| **R1** | Secondary Weapon Mode |
| **Start** | Enter / Accept / Start Game |
| **Select** | Escape / Back / Menu / Pause |

Controls can be remapped in-game.

## Installation

You will need the original Tyrian v2.1 DOS data files. These are legally available as freeware.

1. Copy the `dist` package to the root of your Analogue Pocket SD Card.
2. Download the officially-sanctioned game data used by OpenTyrian:
   [http://camanis.net/tyrian/tyrian21.zip](http://camanis.net/tyrian/tyrian21.zip)
3. Extract the contents of the ZIP file to a folder on your computer.
4. Use the provided Python script to pack these files into a single `.rom` file for the Analogue Pocket:
   ```bash
   python3 bundle_rom.py /path/to/extracted/tyrian21 tyriam.rom
   ```
5. Copy the resulting `tyrian.rom` into your `Assets/tyrian/obsidian.PocketTyrian/` folder on your Analogue Pocket's SD Card.

##Building from Source

To build the firmware yourself, you will need a standard RISC-V GCC toolchain (specifically 
`riscv64-unknown-elf-gcc` with multilib support). You will also need the Intel Quartus FPGA
tools to rebuild the bitfile with the corresponding bootloader image.

1. Clone this repository.
2. Ensure you have the `opentyrian` source submoduled in `third_party/opentyrian`.
3. Navigate to the firmware directory:
   ```bash
   cd tyrian-pocket/src/firmware
   ```
4. Build the binaries:
   ```bash
   make clean && make -j
   ```
5. The resulting `tyrian.bin` is the engine executable that the bootloader runs.
6. Copy the generated bootloader `firmware.mif` to `./src/fpga/core/firmware.mif`
7. Recompile the Quartus project, reverse the resulting bitstream, and copy to the core directory.

## Technical Details & Architecture

This port is technically a fork of [PocketDoom](https://github.com/thinkelastic/PocketDoom). 
*   The system uses a two-stage boot process: a tiny bootloader in FPGA BRAM loads the 300KB `tyrian.bin` engine and the 11MB `tyrian.rom` asset package from the SD card into the SDRAM.
*   The file system (`libc/file.c`) intercepts engine `fopen` calls and routes them to either the read-only asset ROMFS in SDRAM or the read-write nonvolatile RAMFS used for configuration and save games.

## License

*   The OpenTyrian engine is licensed under GPL2
*   The PocketTyrian bootrom code is licensed under MIT
*   See README_POCKETDOOM.md for licensing of the RTL and other components.

## Credits & Acknowledgements

*   **The OpenTyrian Team:** For keeping this classic game alive and accessible.
*   **ThinkElastic / PocketDoom:** For providing the high-quality reference VexRiscv architecture and Analogue OS bridge framework that made this port possible.
*   **Jotego:** For the `jtopl2` FPGA module used to synthesize the soundtrack.
*   **Dyreschlock:** For the `tyrian.bin` platform image used for the core.

