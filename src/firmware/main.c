#include "dataslot.h"
#include "terminal.h"

/* Debug variables (read by misaligned trap handler) */
volatile unsigned int pd_dbg_stage = 0;
volatile unsigned int pd_dbg_info = 0;

/* System registers */
#define SYS_STATUS      (*(volatile unsigned int *)0x40000000)
#define SYS_CYCLE_LO    (*(volatile unsigned int *)0x40000004)

/* Slot IDs from data.json */
#define TYRIAN_BIN_SLOT 1

/* Symbols from linker.ld */
extern char _doom_load_addr[];
extern char _doom_copy_size[];
extern char _doom_bss_start[];
extern char _doom_bss_end[];
extern char _runtime_stack_top[];

extern void tyrian_main(void);
extern void switch_to_runtime_stack_and_call(void (*entry)(void), void *stack_top);

__attribute__((section(".text.boot")))
static void flush_icache(void) {
    __asm__ volatile("fence");
    __asm__ volatile(".word 0x0000100f");  /* fence.i */
}

__attribute__((section(".text.boot")))
static int load_binary(void) {
    uint32_t size = (uint32_t)_doom_copy_size;
    uint32_t done = 0;
    
    while (done < size) {
        uint32_t chunk = (size - done > DMA_CHUNK_SIZE) ? DMA_CHUNK_SIZE : (size - done);
        int rc = dataslot_read(TYRIAN_BIN_SLOT, done, (void*)((uint32_t)_doom_load_addr + done), chunk);
        if (rc != 0) return rc;
        done += chunk;
    }
    return 0;
}

__attribute__((section(".text.boot")))
int main(void) {
    term_init();

    /* Wait for bridge */
    while (!(SYS_STATUS & (1 << 1)));

    /* Load engine */
    if (load_binary() != 0) {
        term_printf("FAILED to load engine\n");
        while(1);
    }

    flush_icache();

    /* Clear SDRAM BSS */
    unsigned int *p = (unsigned int *)_doom_bss_start;
    while (p < (unsigned int *)_doom_bss_end) *p++ = 0;

    /* Jump to Engine at 0x10200000 */
    switch_to_runtime_stack_and_call((void*)0x10200000, _runtime_stack_top);

    while(1);
    return 0;
}
