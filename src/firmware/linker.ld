/*
 * Linker script for Tyrian on Analogue Pocket
 *
 * Two-stage boot:
 * - Bootloader: 64KB BRAM at 0x00000000 (initialized from firmware.mif)
 * - Engine binary: loaded via deferload to SDRAM at 0x10200000, executes in place
 */

ENTRY(_start)

MEMORY {
    BRAM      (rwx) : ORIGIN = 0x00000000, LENGTH = 64K
    SDRAM     (rwx) : ORIGIN = 0x10200000, LENGTH = 48M
}

/* SDRAM layout symbols */
__sdram_base = 0x10000000;
__sdram_size = 64M;

/* Engine binary: VMA = LMA in SDRAM (execute in place, no copy needed) */
__doom_entry = 0x10200000;

SECTIONS {
    /* Global pointer for relaxation */
    . = ALIGN(4);
    PROVIDE(__global_pointer$ = . + 0x800);

    /* --- BRAM SECTIONS (Contiguous for MIF generation) --- */

    /* Bootloader code & data */
    .boot : {
        __boot_start = .;
        KEEP(*(.text.start))       /* Startup code first */
        *(.text.boot*)             /* Bootloader functions */
        *terminal.o(.text*)
        *dataslot.o(.text*)
        *main.o(.text*)
        *misaligned.o(.text*)
        *(.rodata.boot*)
        *terminal.o(.rodata*)
        *dataslot.o(.rodata*)
        *main.o(.rodata*)
        *misaligned.o(.rodata*)
        
        *(.data.boot*)
        *terminal.o(.data*)
        *dataslot.o(.data*)
        *main.o(.data*)
        *misaligned.o(.data*)
        *(.sdata.boot*)
        *terminal.o(.sdata*)
        *dataslot.o(.sdata*)
        *main.o(.sdata*)
        *misaligned.o(.sdata*)
        . = ALIGN(4);
    } > BRAM

    /* Hot code pinned in BRAM */
    .fasttext : {
        . = ALIGN(4);
        __fasttext_start = .;
        KEEP(*(.fasttext*))
        KEEP(*(.fastrodata*))
        . = ALIGN(4);
        __fasttext_end = .;
    } > BRAM

    /* Hot data pinned in BRAM (Initialized) */
    .fastdata : {
        . = ALIGN(4);
        __fastdata_start = .;
        KEEP(*(.fastdata*))
        . = ALIGN(4);
        __fastdata_end = .;
    } > BRAM

    /* --- END OF INITIALIZED BRAM DATA --- */
    __boot_image_end = .;

    /* Hot zero-initialized data pinned in BRAM */
    .fastbss (NOLOAD) : {
        . = ALIGN(4);
        __fastbss_start = .;
        *(.fastbss*)
        . = ALIGN(4);
        __fastbss_end = .;
    } > BRAM

    /* Bootloader BSS (Not in MIF, must be cleared by start.S) */
    .boot_bss (NOLOAD) : {
        . = ALIGN(4);
        __bss_start = .;
        *(.bss.boot*)
        *terminal.o(.bss*)
        *dataslot.o(.bss*)
        *main.o(.bss*)
        *misaligned.o(.bss*)
        *(.sbss.boot*)
        *terminal.o(.sbss*)
        *dataslot.o(.sbss*)
        *main.o(.sbss*)
        *misaligned.o(.sbss*)
        *(COMMON.boot)
        . = ALIGN(4);
        __bss_end = .;
    } > BRAM

    /* === SDRAM sections === */

    /* Engine code section */
    .text __doom_entry : {
        __text_start = .;
        KEEP(*(.text.entry))   /* Force tyrian_main entry to 0x10200000 */
        KEEP(*(.text*))
        KEEP(*(.rodata*))
        KEEP(*(.srodata*))
        . = ALIGN(4);
        __text_end = .;
    } > SDRAM

    /* Initialized data */
    .data : {
        __data_start = .;
        KEEP(*(.data*))
        KEEP(*(.sdata*))
        . = ALIGN(4);
        __data_end = .;
    } > SDRAM

    /* Engine copy info for bootloader */
    __doom_load_addr = __doom_entry;
    __doom_copy_size = __data_end - __text_start;

    /* Uninitialized data (BSS) */
    .doom_bss : {
        . = ALIGN(4);
        __doom_bss_start = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        . = ALIGN(4);
        __doom_bss_end = .;
    } > SDRAM

    /* Memory layout */
    __heap_start = __doom_bss_end;
    __romfs_start = 0x11000000;
    __save_region = 0x13C00000;
    __runtime_stack_top = 0x13000000;
    __runtime_stack_size = 512K;
    __runtime_stack_bottom = __runtime_stack_top - __runtime_stack_size;
    __heap_end = __romfs_start;

    __stack_top = ORIGIN(BRAM) + LENGTH(BRAM) - 16;

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

    /* Keep at least 16 KB BRAM stack/trap headroom after all BRAM sections. */
    ASSERT(__bss_end <= (__stack_top - 16384), "BRAM overflow: insufficient stack headroom")
}
