# PocketDoom

Doom (1993) running natively on the [Analogue Pocket](https://www.analogue.co/pocket) via a VexiiRiscv RISC-V soft CPU on the Cyclone V FPGA. No emulation — the id Software Doom engine runs as bare-metal firmware on a hardware CPU synthesized in the FPGA fabric.

## Installation

1. Copy the contents of the `release/` directory to your Analogue Pocket SD card root
2. Copy your WAD file to `Assets/pocketdoom/common/` on the SD card
3. For link cable multiplayer, you **must** use a **GB/GBC link cable** — GBA cables will NOT work

### Supported WADs

| WAD | Game | Notes |
|-----|------|-------|
| `doom1.wad` | Doom (shareware) | Free, episode 1 only |
| `doom.wad` | Doom (registered) | Episodes 1-3 |
| `doomu.wad` | The Ultimate Doom | Episodes 1-4 |
| `doom2.wad` | Doom II: Hell on Earth | 32 levels |
| `plutonia.wad` | Final Doom: Plutonia Experiment | 32 levels |
| `tnt.wad` | Final Doom: TNT Evilution | 32 levels |

The game mode is auto-detected from WAD contents. If multiple WAD files are present, the Pocket will show a file picker.

See [Installation Layout](#installation-layout) below for the full SD card directory structure.

### Controls

| Button | Action |
|--------|--------|
| D-pad | Move / navigate menus |
| Left stick | Move (forward/back/strafe) |
| A (right face) | Strafe right |
| B (bottom face) | Use / open doors |
| X (top face) | Run |
| Y (left face) | Strafe left |
| R1 / R2 / Right trigger | Fire (Enter in menus) |
| L1 / L2 / Left trigger | Cycle weapon backward (Back/Backspace in menus) |
| Start | Menu (ESC) |
| Select | Automap |

## Features

- **Full Doom engine** — Software-rendered Doom at 320x200, 8-bit indexed color with hardware palette lookup
- **VexiiRiscv RISC-V CPU** — rv32imafc (integer, multiply/divide, atomics, single-precision FPU, compressed) at 100 MHz with AXI4 bus, 64KB I-cache and 64KB D-cache
- **Execute from SDRAM** — Doom code runs directly from SDRAM with AXI4 burst I-cache fills; hot rendering functions pinned in BRAM for zero-latency access
- **Hardware OPL2 synthesizer** — jtopl2 FPGA core replaces software OPL emulation for music playback
- **Non-blocking audio** — Variable-length SFX mixing driven by FIFO level feedback, decoupled from frame rate
- **48 kHz stereo audio** — Hardware mixing of OPL2 music + SFX, I2S output
- **Vsync-locked double buffering** — Direct framebuffer rendering with D-cache flush, swap synchronized to 60 Hz vsync
- **CPU-controlled asset loading** — WAD and doom.bin loaded via deferload (dataslot API) with WAD header parsing to determine file size
- **2-player link cable multiplayer** — GB/GBC link cable at 256 kHz, full-duplex serial protocol
- **Dock support** — HDMI output when docked

## Architecture

```
+-----------------------------------------------------------------------+
|                       Analogue Pocket FPGA                            |
|                     (Cyclone V 5CEBA4F23C8)                           |
+-----------------------------------------------------------------------+
|                                                                       |
|  +-----------------------+    +-----------------------------------+   |
|  |   VexiiRiscv CPU      |    |        Memory Subsystem           |   |
|  |  rv32imafc @ 100 MHz  |    |                                   |   |
|  |                       |    |  +--------+  +--------+  +------+ |   |
|  |  AXI4 bus             |    |  | BRAM   |  | SDRAM  |  | PSRAM| |   |
|  |  3-bus architecture   |    |  | 64KB   |  | 64MB   |  | 16MB | |   |
|  +-----------+-----------+    +--+--------+--+--------+--+------+-+   |
|              |                    |                                    |
|  +-----------+--------------------+----------------------------+      |
|  |                     AXI4 Bus Fabric                         |      |
|  |  FetchL1 (I-cache) / LsuL1 (D-cache) / LsuIO (uncached)   |      |
|  +----+--------+---------+---------+---------+--------+-------+      |
|       |        |         |         |         |        |              |
|  +----+--+ +---+----+ +-+------+ ++------+ ++-----+ ++------+       |
|  | Video | | Audio  | | OPL2   | | Link  | | Sys  | | Text  |       |
|  |Scanout| | Output | | Synth  | | MMIO  | | Regs | | Term  |       |
|  +-------+ +---+----+ +---+----+ +-------+ +------+ +-------+       |
|                |           |                                          |
|                +-----+-----+                                          |
|                      |                                                |
|                 +----+----+                                           |
|                 |   I2S   |                                           |
|                 | 48 kHz  |                                           |
|                 +---------+                                           |
+-----------------------------------------------------------------------+
```

### CPU Architecture

The VexiiRiscv CPU uses a 3-bus AXI4 architecture:

- **FetchL1Axi4** — I-cache reads (16-beat bursts from SDRAM/BRAM)
- **LsuL1Axi4** — D-cache reads/writes (64KB, 2-way set-associative, write-back)
- **LsuPlugin IO** — Uncached single-beat access for MMIO registers

Per-bus address decoding routes transactions to SDRAM, PSRAM, or local peripherals. An AXI4 arbiter multiplexes CPU and video scanout SDRAM access.

## Memory Map

| Address Range             | Size   | Description                                          |
|---------------------------|--------|------------------------------------------------------|
| `0x00000000 - 0x0000FFFF` | 64 KB  | BRAM — bootloader + hot rendering functions          |
| `0x10000000 - 0x100FFFFF` | 1 MB   | Framebuffer 0 (320x240, 8-bit indexed)               |
| `0x10100000 - 0x101FFFFF` | 1 MB   | Framebuffer 1 (double buffer)                        |
| `0x10200000 - 0x1024FFFF` | ~289 KB| Doom code + rodata + data (execute in place)         |
| `0x10250000 - 0x10B7FFFF` | ~9 MB  | BSS + heap                                           |
| `0x10B80000 - 0x10BFFFFF` | 512 KB | Runtime stack                                        |
| `0x10C00000 - 0x13FFFFFF` | ~52 MB | WAD data (loaded via deferload)                      |
| `0x20000000`              | 1.2 KB | Terminal VRAM (40x30 characters)                     |
| `0x40000000`              | 256 B  | System registers                                     |
| `0x4C000000`              | 8 B    | Audio FIFO (write samples / read status)             |
| `0x4D000000`              | 256 B  | Link cable MMIO registers                            |
| `0x4E000000`              | 8 B    | OPL2 hardware registers (addr + data)                |
| `0x50000000 - 0x53FFFFFF` | 64 MB  | SDRAM uncached alias (bypasses D-cache)              |

## System Registers (0x40000000)

| Offset | Register       | Description                                    |
|--------|----------------|------------------------------------------------|
| 0x00   | SYS_STATUS     | [0] sdram_ready, [1] allcomplete               |
| 0x04   | CYCLE_LO       | Cycle counter (low 32 bits)                    |
| 0x08   | CYCLE_HI       | Cycle counter (high 32 bits)                   |
| 0x0C   | DISPLAY_MODE   | 0 = terminal overlay, 1 = framebuffer          |
| 0x10   | FB_DISPLAY     | Display framebuffer address (25-bit word addr) |
| 0x14   | FB_DRAW        | Draw framebuffer address (25-bit word addr)    |
| 0x18   | FB_SWAP        | Write 1 to request swap; read returns pending  |
| 0x40   | PAL_INDEX      | Palette write index (auto-increment)           |
| 0x44   | PAL_DATA       | Palette entry (RGB888, triggers write)         |

## Hardware OPL2 Synthesizer

Doom's music is MUS format, played through OPL2 (YM3812) FM synthesis. The software Nuked OPL3 emulator was too slow on the 100 MHz VexiiRiscv, so synthesis is offloaded to an FPGA OPL2 core ([jotego/jtopl2](https://github.com/jotego/jtopl)).

```
Firmware (RISC-V)              FPGA
+------------------+  MMIO    +------------------+
| MUS parser       |---0x4E-->| opl2_wrapper     |
| opl_write()      | bus stall|   jtopl2 core    |
+------------------+          |   cen ~3.58 MHz  |
                              +--------+---------+
                                       | 16-bit signed
+------------------+          +--------v---------+
| SFX mixer        |---0x4C-->| audio_output     |--> I2S 48 kHz
| I_SubmitSound()  | FIFO push|   HW mix + I2S   |
+------------------+          +------------------+
```

- **Core:** jtopl2 (pure Verilog, proven on Analogue Pocket)
- **Clock enable:** 100 MHz / 28 = 3.571 MHz (~0.22% from ideal 3.58 MHz)
- **MMIO:** `0x4E000000` = register address, `0x4E000004` = register data
- **Bus stalling:** CPU halts automatically during OPL2 write timing (12 cen cycles for addr, 84 for data)
- **Mixing:** Saturating 17-bit add of OPL2 mono + SFX stereo in FPGA, before I2S serialization
- **CDC:** OPL2 audio double-registered from CPU clock to audio clock domain

## Audio Pipeline

- **Sample rate:** 48 kHz stereo, 16-bit signed
- **I2S output:** MCLK 12.288 MHz, SCLK 3.072 MHz, LRCK 48 kHz
- **SFX FIFO:** 4096-entry dual-clock FIFO (CPU clock to audio clock)
- **SFX mixing:** Doom mixes at 11,025 Hz with variable batch size driven by FIFO level feedback; firmware upsamples to 48 kHz with linear interpolation
- **Non-blocking submission:** `I_SubmitSound()` pushes as many samples as the FIFO can accept, called from both `I_StartFrame()` and the main loop to avoid audio gaps
- **Music:** Hardware OPL2 generates audio continuously; mixed with SFX in FPGA before I2S output
- **Interface:** CPU writes SFX samples to `0x4C000000`, OPL2 register writes to `0x4E000000`

## Video Pipeline

- **Resolution:** 320x240 @ 60 Hz (12.288 MHz pixel clock)
- **Color depth:** 8-bit indexed with 256-entry RGB888 hardware palette
- **Scanout:** AXI4 burst reads from SDRAM
- **Double buffered:** Doom renders directly into the SDRAM draw buffer (no memcpy); `FB_SWAP` requests vsync-synchronized swap. Software waits for pending swaps to prevent desynchronization.
- **D-cache coherency:** After rendering, firmware reads 128 KB of unrelated SDRAM to force the 64 KB 2-way D-cache to evict all dirty framebuffer lines to physical SDRAM for video scanout
- **Clock domain crossing:** Dual-clock FIFO between pixel clock (12.288 MHz) and SDRAM clock (100 MHz)

## BRAM Hot Path

Performance-critical rendering inner loops are pinned in the 64 KB BRAM for single-cycle instruction fetch, avoiding I-cache misses during the most executed code paths:

| Function | File | Description |
|----------|------|-------------|
| `R_DrawColumn` | r_draw.c | Wall column rasterizer (innermost loop) |
| `R_DrawSpan` | r_draw.c | Floor/ceiling span rasterizer |
| `FixedMul` | m_fixed.c | 16.16 fixed-point multiply |
| `FixedDiv2` | m_fixed.c | 16.16 fixed-point divide |
| `I_FinishUpdate` | doom_pocket.c | Cache flush + buffer swap |
| `I_SubmitSound` | doom_sound.c | Audio FIFO drain |

Functions are annotated with `PD_FASTTEXT` (defined in `doomdef.h`), placed in the `.fasttext` linker section. Current usage: ~700 bytes of 64 KB BRAM.

## Link Cable Multiplayer

2-player multiplayer over the Analogue Pocket's GB/GBC link cable.

- **Physical:** GB/GBC cable (crosses SO/SI), GBA cables do NOT work
- **Protocol:** Full-duplex serial, 33-bit transfers `{valid, data[31:0]}`, MSB-first
- **Speed:** 256 kHz SCK (GB/GBC mode)
- **Hardware:** TX/RX FIFOs (256 entries each), 3-stage SCK synchronizer for slave mode

## Boot Flow

1. FPGA configures, BRAM bootloader runs from address `0x00000000`
2. Bootloader waits for APF bridge `allcomplete` signal
3. Loads `doom.bin` from data slot 1 into SDRAM (`0x10200000`) via chunked deferload
4. Loads WAD header from data slot 0, parses it to determine full WAD size
5. Loads remaining WAD body into SDRAM (`0x10C00000`)
6. `fence` + `fence.i` (flush D-cache, invalidate I-cache)
7. Clears BSS, switches to runtime stack, jumps to `doom_main()` in SDRAM

## Building

### Prerequisites

- **RISC-V toolchain:** `riscv64-elf-gcc` with rv32imafc support
- **Intel Quartus Prime:** 25.1 or later (Lite edition sufficient)
- **Analogue Pocket:** Firmware 2.2 or later

```bash
# Arch Linux
sudo pacman -S riscv64-elf-gcc riscv64-elf-newlib

# The firmware uses -march=rv32imafc -mabi=ilp32f
```

### Build Firmware

```bash
cd src/firmware
make                  # Builds doom.bin + firmware.mif
make install          # Copies MIF to FPGA directory
```

### Build FPGA

```bash
cd src/fpga
make                  # Full Quartus synthesis
make mif              # Update MIF only, no resynthesis (~1 min)
make program          # Program via JTAG (USB Blaster)
```

### Package Release

```bash
make                  # From project root — packages release/ directory
```

### Quick Development Cycle

```bash
cd src/fpga
make quick            # Build firmware + update MIF + program via JTAG
```

### Deploy to SD Card

```bash
./deploy.sh           # Auto-detects SD card, mounts, syncs release/, unmounts
```

## Installation Layout

```
SD Card Root/
+-- Assets/
|   +-- pocketdoom/
|       +-- common/
|           +-- doom.bin
|           +-- doom1.wad        (shareware or doom.wad for registered)
+-- Cores/
|   +-- ThinkElastic.PocketDoom/
|       +-- bitstream.rbf_r
|       +-- core.json
|       +-- (other .json files)
+-- Platforms/
    +-- _images/
    |   +-- pocketdoom.bin
    +-- pocketdoom.json
```

## Project Structure

```
.
+-- src/
|   +-- firmware/                  # Doom firmware (C, bare-metal)
|   |   +-- main.c                 # Bootloader (deferload, BSS clear, jump)
|   |   +-- doom/                  # Doom engine source
|   |   |   +-- riscv/d_main.c    # Platform-specific main
|   |   |   +-- doomdef.h         # PD_FASTTEXT/PD_FASTDATA macros
|   |   +-- doom_pocket.c         # Platform layer (video, input, timing)
|   |   +-- doom_sound.c          # SFX audio driver (async FIFO submission)
|   |   +-- doom_music.c          # MUS parser + hardware OPL2 driver
|   |   +-- doom_net.c            # Link cable network driver
|   |   +-- libc/                 # Minimal C library
|   |   +-- linker.ld             # Linker script (BRAM/SDRAM layout)
|   |   +-- Makefile
|   |
|   +-- fpga/                     # FPGA design (Verilog)
|   |   +-- core/
|   |   |   +-- core_top.v        # Top-level: CPU + bus + peripherals
|   |   |   +-- cpu_system.v      # VexiiRiscv + AXI4 bus routing
|   |   |   +-- axi_periph_slave.v # BRAM, system regs, palette, controllers
|   |   |   +-- axi_sdram_slave.v  # SDRAM AXI4 slave with burst support
|   |   |   +-- axi_sdram_arbiter.v # CPU + video scanout SDRAM arbitration
|   |   |   +-- opl2_wrapper.v    # Hardware OPL2 peripheral (bus-stalling)
|   |   |   +-- audio_output.v    # I2S output with OPL2+SFX hardware mixing
|   |   |   +-- io_sdram.v        # SDRAM controller
|   |   |   +-- psram_controller.v # PSRAM controller
|   |   |   +-- video_scanout_indexed.v  # 8-bit indexed video scanout
|   |   |   +-- link_mmio.v       # Link cable serial transceiver
|   |   |   +-- text_terminal.v   # Debug text overlay
|   |   +-- jtopl/                # Vendored jtopl2 OPL2 core (jotego, GPL-3.0)
|   |   +-- vexriscv/
|   |   |   +-- VexiiRiscv_Full.v # Generated RISC-V CPU core
|   |   +-- apf/                  # Analogue Pocket framework (bridge, I/O)
|   |   +-- Makefile
|   |
|   +-- firmware_test/            # Hardware test firmware (SDRAM/PSRAM/CPU tests)
|
+-- dist/                         # Platform images and icons
+-- release/                      # Packaged release for SD card
+-- tools/
|   +-- capture_ocr.sh            # HDMI capture + OCR testing tool
+-- Makefile                      # Top-level build/package
+-- deploy.sh                     # Quick deploy to SD card (auto-detect + mount)
+-- *.json                        # APF configuration files
```

## Important Notes

- **JTAG programming loses SDRAM data.** After JTAG programming, the Pocket must reload `doom.bin` and the WAD from the SD card. Always deploy both firmware and bitstream to the SD card for testing.
- **Firmware and FPGA must match.** The BRAM initialization (MIF) is compiled into the bitstream. If `doom.bin` on the SD card doesn't match the MIF in the FPGA, `.fasttext` function calls will jump to wrong addresses and crash.

## License

- **Doom engine:** GPL-2.0 (id Software)
- **jtopl2 OPL2 core:** GPL-3.0 (Jose Tejada / jotego)
- **VexiiRiscv:** MIT (SpinalHDL)
- **PocketDoom (FPGA/firmware):** MIT

## Acknowledgments

- [id Software](https://github.com/id-Software/DOOM) — Original Doom source release
- [jotego/jtopl](https://github.com/jotego/jtopl) — Hardware OPL2 synthesizer core
- [SpinalHDL/VexiiRiscv](https://github.com/SpinalHDL/VexiiRiscv) — RISC-V CPU core
- [Analogue](https://www.analogue.co/developer) — Pocket openFPGA development framework
