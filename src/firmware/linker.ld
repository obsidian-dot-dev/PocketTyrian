/*
 * Linker script for PocketDoom on Analogue Pocket
 *
 * Two-stage boot:
 * - Bootloader: 64KB BRAM at 0x00000000 (initialized from firmware.mif)
 * - Doom binary: loaded via deferload to SDRAM at 0x10200000, executes in place
 * - WAD data: loaded via deferload to SDRAM at 0x10C00000
 *
 * Memory layout:
 * - BRAM:     0x00000000 (64KB)  - Bootloader code, stack, hot functions
 * - SDRAM:    0x10000000 (64MB)
 *   - 0x10000000: Framebuffer 0 (320x240, 8-bit indexed)
 *   - 0x10100000: Framebuffer 1 (double buffer)
 *   - 0x10200000: doom.bin code + rodata + data (execute in place from SDRAM)
 *   - After code: BSS + Heap + Stack (WAD loaded on-demand via dataslot)
 * - Terminal: 0x20000000 (8KB)   - Character VRAM
 * - SysRegs:  0x40000000         - System control registers
 */

ENTRY(_start)

MEMORY {
    BRAM      (rwx) : ORIGIN = 0x00000000, LENGTH = 64K
    SDRAM     (rwx) : ORIGIN = 0x10200000, LENGTH = 48M    /* Doom code + BSS + Heap (WAD loaded on-demand) */
}

/* SDRAM layout symbols */
__sdram_base = 0x10000000;
__sdram_size = 64M;

/* Doom binary: VMA = LMA in SDRAM (execute in place, no copy needed) */
__doom_entry = 0x10200000;

SECTIONS {
    /* Bootloader code - in BRAM, runs at startup */
    .boot : {
        KEEP(*(.text.start))       /* Startup code first */
        *(.text.boot*)             /* Bootloader functions */
        terminal.o(.text*)         /* Terminal functions for boot diagnostics */
        terminal.o(.rodata*)
        terminal.o(.srodata*)
        dataslot.o(.text*)         /* Dataslot functions for loading */
        dataslot.o(.rodata*)
        dataslot.o(.srodata*)
        main.o(.srodata*)          /* Boot test float constants */
        *(.rodata.boot*)
        . = ALIGN(4);
    } > BRAM

    /* Bootloader data */
    .boot_data : {
        *(.data.boot*)
        *(.sdata.boot*)
        . = ALIGN(4);
    } > BRAM

    /* Bootloader BSS */
    .boot_bss : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss.boot*)
        *(.sbss.boot*)
        . = ALIGN(4);
        __bss_end = .;
    } > BRAM

    /* Hot code/data pinned in BRAM for lower latency.
     * Included in boot image generation (firmware.mif). */
    .fasttext : {
        . = ALIGN(4);
        __fasttext_start = .;
        KEEP(*(.fasttext*))
        KEEP(*(.fastrodata*))
        . = ALIGN(4);
        __fasttext_end = .;
    } > BRAM

    .fastdata : {
        . = ALIGN(4);
        __fastdata_start = .;
        KEEP(*(.fastdata*))
        . = ALIGN(4);
        __fastdata_end = .;
    } > BRAM

    /* === SDRAM sections: Doom code executes in place from SDRAM === */

    /* Doom code section - VMA = LMA in SDRAM (no copy needed) */
    .text __doom_entry : {
        __text_start = .;
        KEEP(*(.text*))        /* All code (except boot) - KEEP to prevent gc */
        KEEP(*(.rodata*))      /* Read-only data */
        KEEP(*(.srodata*))     /* Small read-only data (float constants) */
        . = ALIGN(4);
        __text_end = .;
    } > SDRAM

    /* Doom initialized data - contiguous with .text in SDRAM */
    .data : {
        __data_start = .;
        KEEP(*(.data*))
        KEEP(*(.sdata*))
        . = ALIGN(4);
        __data_end = .;
    } > SDRAM

    /* doom.bin = .text + .data, loaded by deferload directly to SDRAM */
    __doom_load_addr = __doom_entry;
    __doom_copy_size = __data_end - __text_start;

    /* Doom uninitialized data (BSS) - in SDRAM after code+data */
    .doom_bss : {
        . = ALIGN(4);
        __doom_bss_start = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        . = ALIGN(4);
        __doom_bss_end = .;
    } > SDRAM

    /* Heap starts after BSS in SDRAM region.
     * Reserve a dedicated runtime stack at top of SDRAM to keep game stack
     * away from BRAM boot code. */
    __heap_start = __doom_bss_end;
    __runtime_stack_top = 0x14000000;          /* Near top of 64MB SDRAM */
    __runtime_stack_size = 512K;               /* 512 KB runtime stack */
    __runtime_stack_bottom = __runtime_stack_top - __runtime_stack_size;
    __heap_end = __runtime_stack_bottom;

    /* Stack at end of BRAM (grows downward) */
    __stack_top = ORIGIN(BRAM) + LENGTH(BRAM);

    /* Provide symbols for C code */
    PROVIDE(_start_addr = ORIGIN(BRAM));
    PROVIDE(_stack_top = __stack_top);
    PROVIDE(_bss_start = __bss_start);
    PROVIDE(_bss_end = __bss_end);
    PROVIDE(_doom_bss_start = __doom_bss_start);
    PROVIDE(_doom_bss_end = __doom_bss_end);
    PROVIDE(_heap_start = __heap_start);
    PROVIDE(_heap_end = __heap_end);
    PROVIDE(_runtime_stack_top = __runtime_stack_top);
    PROVIDE(_doom_entry = __doom_entry);
    PROVIDE(_doom_load_addr = __doom_load_addr);
    PROVIDE(_doom_copy_size = __doom_copy_size);

    /* Keep at least 8 KB BRAM stack/trap headroom after hot sections. */
    ASSERT(__fastdata_end <= (__stack_top - 8192), "BRAM overflow: insufficient stack headroom")
}
