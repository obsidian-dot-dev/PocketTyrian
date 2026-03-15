#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#include "SDL.h"
#include "sdl_pocket.h"
#include "opentyr.h"
#include "palette.h"
#include "video.h"
#include "dataslot.h"
#include "terminal.h"

/* Hardware Register Definitions */
#define SYSREG_BASE         0x40000000
#define SYS_FB_DRAW         (*(volatile uint32_t *)(SYSREG_BASE + 0x14))
#define SYS_FB_SWAP         (*(volatile uint32_t *)(SYSREG_BASE + 0x18))
#define SYS_DISPLAY_MODE    (*(volatile uint32_t *)(SYSREG_BASE + 0x0C))
#define SYS_PAL_INDEX       (*(volatile uint32_t *)(SYSREG_BASE + 0x40))
#define SYS_PAL_DATA        (*(volatile uint32_t *)(SYSREG_BASE + 0x44))

#define SDRAM_BASE          0x10000000
#define SDRAM_UNCACHED_BASE 0x50000000

/* Memory Layout */
#define ROMFS_SDRAM_ADDR    0x11000000
#define ROMFS_SLOT_ID       0
#define ROMFS_TOTAL_SIZE    11487952

#define ADDR_VGASCREEN      (SDRAM_BASE + 0x020000)
#define ADDR_VGASCREEN2     (SDRAM_BASE + 0x040000)
#define ADDR_GAMESCREEN     (SDRAM_BASE + 0x060000)

/* ============================================
 * Internal Platform Helpers
 * ============================================ */

static void flush_dcache(void) {
    for (int i = 0; i < 1024; i++) __asm__ volatile("fence");
}

static SDL_Surface *internal_create_surface(void *pixels, int w, int h) {
    SDL_Surface *s = malloc(sizeof(SDL_Surface));
    s->format = malloc(sizeof(SDL_PixelFormat));
    s->format->palette = malloc(sizeof(SDL_Palette));
    s->format->palette->ncolors = 256;
    s->format->palette->colors = malloc(256 * sizeof(SDL_Color));
    s->format->BitsPerPixel = 8;
    s->format->BytesPerPixel = 1;
    s->w = w;
    s->h = h;
    s->pitch = w;
    s->pixels = pixels;
    s->userdata = NULL;
    return s;
}

void update_hardware_palette(SDL_Color *pal) {
    SYS_PAL_INDEX = 0;
    for (int i = 0; i < 256; i++) {
        SYS_PAL_DATA = (pal[i].r << 16) | (pal[i].g << 8) | pal[i].b;
    }
}

/* ============================================
 * OpenTyrian Integration
 * ============================================ */

void JE_showVGA(void) {
    extern Palette palette;
    update_hardware_palette(palette);

    uint32_t hw_word_offset = SYS_FB_DRAW;
    uint8_t *hw_ptr = (uint8_t *)(SDRAM_UNCACHED_BASE + (hw_word_offset << 1));

    extern SDL_Surface *VGAScreen;
    if (VGAScreen && VGAScreen->pixels) {
        memcpy(hw_ptr, VGAScreen->pixels, 320 * 200);
    }

    flush_dcache();
    SYS_FB_SWAP = 1;

    /* Service audio FIFO while waiting for vsync swap */
    service_audio();
    while (SYS_FB_SWAP) {
        service_audio();
    }
}

void init_pocket_video(void) {
    extern SDL_Surface *VGAScreen, *VGAScreenSeg, *VGAScreen2, *game_screen;
    extern SDL_PixelFormat *main_window_tex_format;

    VGAScreen = VGAScreenSeg = internal_create_surface((void*)ADDR_VGASCREEN, 320, 200);
    VGAScreen2 = internal_create_surface((void*)ADDR_VGASCREEN2, 320, 200);
    game_screen = internal_create_surface((void*)ADDR_GAMESCREEN, 320, 200);

    main_window_tex_format = VGAScreen->format;

    memset((void*)SDRAM_UNCACHED_BASE, 0, 0x200000);
    SYS_DISPLAY_MODE = 1;
}

void init_video(void) { init_pocket_video(); }

/* ============================================
 * Entry Point
 * ============================================ */

extern int opentyrian_main(int argc, char *argv[]);
extern void heap_init(void *ptr, size_t size);
extern char _heap_start[], _heap_end[];
extern void rom_fs_init(void);

__attribute__((section(".text.entry")))
void tyrian_main(void) {
    term_clear();
    heap_init(_heap_start, _heap_end - _heap_start);

    /* Setup a nice loading box */
    term_setpos(0, 1);
    term_printf("===============================");
    term_setpos(1, 1);
    term_printf("          PocketTyrian         ");
    term_setpos(2, 1);
    term_printf("OpenTyrian port by Obsidian.dev");
    term_setpos(3, 1);
    term_printf("===============================");

    term_setpos(5, 1);
    term_printf("[                            ]");

    const char* fun_messages[] = {
        "Reticulating splines...",
        "Warming up OPL2...     ",
        "Priming warp plasma... ",
        "Sorting 1s and 0s...   ",
        "Mining data cubes...   ",
        "Compiling shaders...   ",
        "Hogging glory...       ",
        "Preparing taxes...     "
    };

    uint32_t done = 0;
    int progress_steps = 0;
    int max_steps = 28;
    int msg_idx = 0;

    while (done < ROMFS_TOTAL_SIZE) {
        uint32_t chunk = (ROMFS_TOTAL_SIZE - done > 256 * 1024) ? 256 * 1024 : (ROMFS_TOTAL_SIZE - done);
        dataslot_read(ROMFS_SLOT_ID, done, (void*)(ROMFS_SDRAM_ADDR + done), chunk);
        done += chunk;

        int new_steps = (done * max_steps) / ROMFS_TOTAL_SIZE;
        if (new_steps > progress_steps) {
            term_setpos(5, 2 + progress_steps);
            for(int i = progress_steps; i < new_steps; i++) {
                term_printf("=");
            }
            progress_steps = new_steps;

            /* Update random message every few steps */
            if (progress_steps % 4 == 0) {
                term_setpos(7, 1);
                term_printf("%s", fun_messages[msg_idx % 8]);
                msg_idx++;
            }
        }
    }

    term_setpos(7, 1);
    term_printf("%s", "Launch sequence initiated!");

    rom_fs_init();
    char *argv[] = {"opentyrian", NULL};
    opentyrian_main(1, argv);
}
