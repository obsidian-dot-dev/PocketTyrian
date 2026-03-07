/*
 * Doom platform layer for Analogue Pocket (PocketDoom)
 *
 * Implements the Doom I/O interfaces (i_video, i_system) using:
 * - SDRAM double-buffered 8-bit indexed framebuffer with hardware palette
 * - 110 MHz cycle counter for timing
 * - Analogue Pocket controller input via MMIO registers
 */

#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "doomdef.h"
#include "doomstat.h"
#include "d_main.h"
#include "d_event.h"
#include "d_player.h"
#include "g_game.h"
#include "m_misc.h"
#include "i_system.h"
#include "i_sound.h"
#include "i_video.h"
#include "v_video.h"

/* From doom_sound.c — non-blocking audio FIFO drain */
extern void I_SubmitSound(void);

/* ============================================
 * Hardware register definitions
 * ============================================ */

#define SYS_BASE            0x40000000
#define SYS_STATUS          (*(volatile uint32_t *)(SYS_BASE + 0x00))
#define SYS_CYCLE_LO        (*(volatile uint32_t *)(SYS_BASE + 0x04))
#define SYS_CYCLE_HI        (*(volatile uint32_t *)(SYS_BASE + 0x08))
#define SYS_DISPLAY_MODE    (*(volatile uint32_t *)(SYS_BASE + 0x0C))
#define SYS_FB_DISPLAY      (*(volatile uint32_t *)(SYS_BASE + 0x10))
#define SYS_FB_DRAW         (*(volatile uint32_t *)(SYS_BASE + 0x14))
#define SYS_FB_SWAP         (*(volatile uint32_t *)(SYS_BASE + 0x18))
#define SYS_PAL_INDEX       (*(volatile uint32_t *)(SYS_BASE + 0x40))
#define SYS_PAL_DATA        (*(volatile uint32_t *)(SYS_BASE + 0x44))
#define SYS_CONT1_KEY       (*(volatile uint32_t *)(SYS_BASE + 0x50))
#define SYS_CONT1_JOY       (*(volatile uint32_t *)(SYS_BASE + 0x54))
#define SYS_CONT1_TRIG      (*(volatile uint32_t *)(SYS_BASE + 0x58))

/* SDRAM framebuffer addresses (CPU byte addresses) */
#define SDRAM_BASE          0x10000000
#define FB0_ADDR            (SDRAM_BASE + 0x000000)  /* Framebuffer 0 */
#define FB1_ADDR            (SDRAM_BASE + 0x100000)  /* Framebuffer 1 */
#define FB_STRIDE           320                       /* Bytes per line */
#define FB_LINES            240                       /* Total scanout lines */

/* Convert SDRAM word address from register to CPU byte address */
#define FB_REG_TO_CPU(reg)  (SDRAM_BASE + ((reg) << 1))

/* Cache eviction region: 128KB of SDRAM not used by framebuffers or game data.
 * Reading this forces the 64KB 2-way D-cache to evict all dirty FB lines. */
#define CACHE_FLUSH_ADDR    0x10380000
#define CACHE_FLUSH_SIZE    (128 * 1024)

/* CPU clock frequency */
#define CPU_HZ              100000000
#define TICRATE             35
#define CYCLES_PER_TIC      (CPU_HZ / TICRATE)

/* Analogue Pocket controller button bits (active high) */
#define PAD_DPAD_UP         (1 << 0)
#define PAD_DPAD_DOWN       (1 << 1)
#define PAD_DPAD_LEFT       (1 << 2)
#define PAD_DPAD_RIGHT      (1 << 3)
#define PAD_FACE_A          (1 << 4)
#define PAD_FACE_B          (1 << 5)
#define PAD_FACE_X          (1 << 6)
#define PAD_FACE_Y          (1 << 7)
#define PAD_TRIG_L1         (1 << 8)
#define PAD_TRIG_R1         (1 << 9)
#define PAD_TRIG_L2         (1 << 10)
#define PAD_TRIG_R2         (1 << 11)
#define PAD_TRIG_L3         (1 << 12)
#define PAD_TRIG_R3         (1 << 13)
#define PAD_FACE_SELECT     (1 << 14)
#define PAD_FACE_START      (1 << 15)
/* Analog trigger press threshold (0..255) */
#define TRIG_PRESS_THRESHOLD 64

/* ============================================
 * Entry point (called by bootloader)
 * ============================================ */

/* Linker symbols for heap region */
extern char _heap_start[], _heap_end[];

void doom_main(void)
{
    /* Initialize heap allocator (must happen before any malloc) */
    heap_init(_heap_start, _heap_end - _heap_start);
    printf("doom_main: heap %x-%x (%d bytes)\n",
           (unsigned)_heap_start, (unsigned)_heap_end,
           (int)(_heap_end - _heap_start));

    D_DoomMain();  /* Never returns */
}

/* ============================================
 * Video interface (i_video.h)
 * ============================================ */

void I_InitGraphics(void)
{
    /* Set default gamma */
    usegamma = 1;

    /* Clear both framebuffers to black via uncached alias so the
     * video scanout hardware sees zeros immediately. */
    memset((void *)(0x50000000 + 0x000000), 0, FB_STRIDE * FB_LINES);
    memset((void *)(0x50000000 + 0x100000), 0, FB_STRIDE * FB_LINES);

    /* Point screens[0] at the current draw framebuffer (cached SDRAM).
     * Doom will render directly into it — no memcpy needed in I_FinishUpdate. */
    uint32_t draw_reg = SYS_FB_DRAW;
    screens[0] = (byte *)FB_REG_TO_CPU(draw_reg);

    /* Switch to framebuffer display mode */
    SYS_DISPLAY_MODE = 1;
}

void I_ShutdownGraphics(void)
{
}

void I_SetPalette(byte *palette)
{
    /* Write all 256 palette entries via auto-incrementing registers */
    SYS_PAL_INDEX = 0;
    for (int i = 0; i < 256; i++) {
        byte r = gammatable[usegamma][*palette++];
        byte g = gammatable[usegamma][*palette++];
        byte b = gammatable[usegamma][*palette++];
        SYS_PAL_DATA = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    }
}

void I_UpdateNoBlit(void)
{
}

PD_FASTTEXT void I_FinishUpdate(void)
{
    /* screens[0] points directly at the draw framebuffer in cached SDRAM.
     * Doom has already rendered into it.  Force D-cache writeback by reading
     * 128KB of unrelated SDRAM — this evicts all dirty FB lines from the
     * 64KB 2-way D-cache, flushing them to physical SDRAM where the video
     * scanout can read them. */
    volatile uint32_t *flush = (volatile uint32_t *)CACHE_FLUSH_ADDR;
    volatile uint32_t sink;
    for (int i = 0; i < (int)(CACHE_FLUSH_SIZE / 4); i++)
        sink = flush[i];
    (void)sink;

    /* Wait for any previous swap to complete before requesting a new one.
     * Without this, the software buffer toggle can desync from hardware
     * if rendering is faster than vsync (60 Hz). */
    while (SYS_FB_SWAP)
        ;

    /* Request buffer swap on next vsync */
    SYS_FB_SWAP = 1;

    /* Read the actual draw buffer from hardware to stay in sync.
     * After swap request, hardware still reports the pre-swap draw buffer
     * (swap is pending until vsync), but since we just waited for the
     * previous swap to complete, the toggle is guaranteed 1:1 with vsync. */
    uint32_t cur_draw = (uint32_t)screens[0];
    screens[0] = (byte *)(cur_draw == FB0_ADDR ? FB1_ADDR : FB0_ADDR);
}

void I_WaitVBL(int count)
{
    /* Simple delay - wait for approximately count vblanks (60 Hz) */
    uint32_t cycles_per_vbl = CPU_HZ / 60;
    uint32_t start = SYS_CYCLE_LO;
    while ((SYS_CYCLE_LO - start) < (uint32_t)count * cycles_per_vbl)
        ;
}

void I_ReadScreen(byte *scr)
{
    memcpy(scr, screens[0], SCREENWIDTH * SCREENHEIGHT);
}

/* ============================================
 * System interface (i_system.h)
 * ============================================ */

void I_Init(void)
{
    I_InitSound();
    I_InitMusic();
}

byte *I_ZoneBase(int *size)
{
    /* Give 6MB to Doom's zone allocator */
    *size = 6 * 1024 * 1024;
    return (byte *)malloc(*size);
}

int I_GetTime(void)
{
    /* Read 64-bit cycle counter, convert to 35 Hz tics */
    uint32_t lo = SYS_CYCLE_LO;
    uint32_t hi = SYS_CYCLE_HI;
    uint64_t cycles = ((uint64_t)hi << 32) | lo;
    return (int)(cycles / CYCLES_PER_TIC);
}

/* Controller state tracking */
static uint32_t prev_buttons = 0;

/* Button-to-key mapping */
static const struct {
    uint32_t button;
    int key;
} button_map[] = {
    { PAD_DPAD_UP,      KEY_UPARROW },
    { PAD_DPAD_DOWN,    KEY_DOWNARROW },
    { PAD_DPAD_LEFT,    KEY_LEFTARROW },
    { PAD_DPAD_RIGHT,   KEY_RIGHTARROW },
    { PAD_FACE_A,       '.' },          /* Strafe right */
    { PAD_FACE_B,       ' ' },          /* Use / Open */
    { PAD_FACE_X,       KEY_RSHIFT },   /* Run */
    { PAD_FACE_Y,       ',' },          /* Strafe left */
    { PAD_FACE_START,   KEY_ESCAPE },   /* Menu */
    { PAD_FACE_SELECT,  KEY_TAB },      /* Automap */
};

#define NUM_BUTTONS (sizeof(button_map) / sizeof(button_map[0]))

/* Weapon cycle key posted on previous frame (0 = none) */
static int weapon_cycle_key = 0;
/* Virtual button states derived from digital + analog trigger sources */
static int fire_button_down = 0;
static int menu_back_down = 0;
static int menu_enter_down = 0;
static int weapon_cycle_down = 0;

void I_StartFrame(void)
{
    /* Drain pending audio into the FIFO at the top of the frame too,
     * so submission isn't limited to one chance per loop iteration. */
    I_SubmitSound();
}

void I_StartTic(void)
{
    event_t event;
    uint32_t buttons = SYS_CONT1_KEY;
    uint32_t changed = buttons ^ prev_buttons;
    uint32_t trig = SYS_CONT1_TRIG;
    int ltrig_down = ((trig & 0xFFu) >= TRIG_PRESS_THRESHOLD);
    int rtrig_down = (((trig >> 8) & 0xFFu) >= TRIG_PRESS_THRESHOLD);
    int l1_down = (buttons & PAD_TRIG_L1) != 0;
    int r1_down = (buttons & PAD_TRIG_R1) != 0;
    int r2_down = (buttons & PAD_TRIG_R2) != 0;
    int l2_down = (buttons & PAD_TRIG_L2) != 0;
    /* R2/right trigger: fire in gameplay, accept in menus. L2/left trigger: weapon cycle in gameplay, back in menus. */
    int fire_down = (!menuactive && (r2_down || rtrig_down || r1_down));
    int enter_down = (menuactive && (r2_down || rtrig_down || r1_down));
    int back_down = (menuactive && (l2_down || ltrig_down || l1_down));
    int cycle_down = (!menuactive && (l2_down || ltrig_down || l1_down));

    /* Release the synthetic weapon key from last frame */
    if (weapon_cycle_key) {
        event.type = ev_keyup;
        event.data1 = weapon_cycle_key;
        event.data2 = 0;
        event.data3 = 0;
        D_PostEvent(&event);
        weapon_cycle_key = 0;
    }

    for (unsigned i = 0; i < NUM_BUTTONS; i++) {
        uint32_t mask = button_map[i].button;
        if (changed & mask) {
            event.type = (buttons & mask) ? ev_keydown : ev_keyup;
            event.data1 = button_map[i].key;
            event.data2 = 0;
            event.data3 = 0;
            D_PostEvent(&event);
        }
    }

    /* Fire press/release from right shoulder/trigger sources. */
    if (fire_down != fire_button_down) {
        event.type = fire_down ? ev_keydown : ev_keyup;
        event.data1 = KEY_RCTRL;
        event.data2 = 0;
        event.data3 = 0;
        D_PostEvent(&event);
        fire_button_down = fire_down;
    }

    /* Menu back press/release from right shoulder only when menu is active. */
    if (back_down != menu_back_down) {
        event.type = back_down ? ev_keydown : ev_keyup;
        event.data1 = KEY_BACKSPACE;
        event.data2 = 0;
        event.data3 = 0;
        D_PostEvent(&event);
        menu_back_down = back_down;
    }

    /* Menu enter press/release from left shoulder only when menu is active. */
    if (enter_down != menu_enter_down) {
        event.type = enter_down ? ev_keydown : ev_keyup;
        event.data1 = KEY_ENTER;
        event.data2 = 0;
        event.data3 = 0;
        D_PostEvent(&event);
        menu_enter_down = enter_down;
    }

    /* L2 pressed (digital or analog): cycle to next owned weapon */
    if (cycle_down && !weapon_cycle_down) {
        player_t *p = &players[consoleplayer];
        int cur = (int)p->readyweapon;
        int next;
        for (next = cur - 1; next != cur; next--) {
            if (next < 0) next = NUMWEAPONS - 1;
            if (p->weaponowned[next])
                break;
        }
        weapon_cycle_key = '1' + next;
        event.type = ev_keydown;
        event.data1 = weapon_cycle_key;
        event.data2 = 0;
        event.data3 = 0;
        D_PostEvent(&event);
    }
    weapon_cycle_down = cycle_down;

    prev_buttons = buttons;
}

ticcmd_t *I_BaseTiccmd(void)
{
    static ticcmd_t emptycmd;
    return &emptycmd;
}

void I_Quit(void)
{
    D_QuitNetGame();
    M_SaveDefaults();
    I_ShutdownGraphics();
    exit(0);
}

byte *I_AllocLow(int length)
{
    byte *mem = (byte *)malloc(length);
    if (mem)
        memset(mem, 0, length);
    return mem;
}

void I_Tactile(int on, int off, int total)
{
    on = off = total = 0;
}

void I_Error(char *error, ...)
{
    va_list argptr;

    /* Switch back to terminal so error is visible */
    SYS_DISPLAY_MODE = 0;

    va_start(argptr, error);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, error, argptr);
    fprintf(stderr, "\n");
    va_end(argptr);

    fflush(stderr);

    if (demorecording)
        G_CheckDemoStatus();

    D_QuitNetGame();
    I_ShutdownGraphics();
    exit(-1);
}
