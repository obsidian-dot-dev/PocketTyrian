#include "dataslot.h"
#include "libc/libc.h"
#include "terminal.h"

/* Set to 1 for verbose dataslot debug output */
#define DS_DEBUG 0
#if DS_DEBUG
#define DS_LOG(...) term_printf(__VA_ARGS__)
#else
#define DS_LOG(...) do {} while(0)
#endif

/* Address translation */
#undef CPU_TO_BRIDGE_ADDR
#define CPU_TO_BRIDGE_ADDR(addr) ((addr) - 0x10000000)

/* Status and Commands are already defined in dataslot.h via SYS_BASE (0x40000000) */

__attribute__((section(".text.boot")))
int dataslot_wait_complete(void) {
    /* Wait for DONE bit to be set */
    int timeout = 10000000;
    while (!(DS_STATUS & DS_STATUS_DONE)) {
        if (--timeout <= 0) {
            DS_LOG("wait: timeout at done, t=%d s=%x\n", timeout, DS_STATUS);
            return -1;
        }
    }

    /* Check error code */
    uint32_t final_status = DS_STATUS;
    int err = (final_status & DS_STATUS_ERR_MASK) >> DS_STATUS_ERR_SHIFT;
    DS_LOG("wait: final status=%x err=%d\n", final_status, err);

    /* Mandatory delay to allow FPGA bridge state machine to reset */
    for (volatile int i = 0; i < 1000; i++) {}

    return err ? -err : 0;
}

__attribute__((section(".text.boot")))
int dataslot_read(uint32_t slot_id, uint32_t offset, void *dest, uint32_t length) {
    /* Validate destination is in SDRAM */
    uint32_t dest_addr = (uint32_t)dest;
    if (dest_addr < 0x10000000 || dest_addr >= 0x14000000) {
        return -10;
    }

    uint32_t bridge_addr = CPU_TO_BRIDGE_ADDR(dest_addr);

    /* Debug: print parameters */
    DS_LOG("DS: slot=%d off=%x br=%x len=%x\n",
                slot_id, offset, bridge_addr, length);

    /* Mandatory delay to allow bridge to be ready */
    for (volatile int i = 0; i < 1000; i++) {}

    /* Flush D-cache */
    __asm__ volatile("fence");

    /* Set up registers */
    DS_SLOT_ID = slot_id;
    DS_SLOT_OFFSET = offset;
    DS_BRIDGE_ADDR = bridge_addr;
    DS_LENGTH = length;

    /* Trigger read command */
    DS_COMMAND = DS_CMD_READ;

    /* Wait for completion */
    int result = dataslot_wait_complete();

    /* CDC sync delay */
    for (volatile int i = 0; i < 64; i++) {}

    return result;
}

__attribute__((section(".text.boot")))
int dataslot_write(uint16_t slot_id, uint32_t offset, const void *src, uint32_t length) {
    /* Validate source is in SDRAM */
    uint32_t src_addr = (uint32_t)src;
    if (src_addr < 0x10000000 || src_addr >= 0x14000000) {
        return -10;
    }

    uint32_t bridge_addr = CPU_TO_BRIDGE_ADDR(src_addr);

    /* Mandatory delay */
    for (volatile int i = 0; i < 1000; i++) {}

    /* Ensure D-cache data is in SDRAM */
    __asm__ volatile("fence");

    /* Set up registers */
    DS_SLOT_ID = slot_id;
    DS_SLOT_OFFSET = offset;
    DS_BRIDGE_ADDR = bridge_addr;
    DS_LENGTH = length;

    /* Trigger write command */
    DS_COMMAND = DS_CMD_WRITE;

    /* Wait for completion */
    return dataslot_wait_complete();
}

__attribute__((section(".text.boot")))
int dataslot_open_file(const char *filename, uint32_t flags, uint32_t size) {
    (void)filename; (void)flags; (void)size;
    return -1;
}

int dataslot_get_size(uint16_t slot_id, uint32_t *size_out) {
    switch (slot_id) {
        case 0: *size_out = 16 * 1024 * 1024; break;
        case 1: *size_out = 4 * 1024 * 1024; break;
        case 5: *size_out = 64 * 1024; break;
        default: *size_out = 1 * 1024 * 1024; break;
    }
    return 0;
}
