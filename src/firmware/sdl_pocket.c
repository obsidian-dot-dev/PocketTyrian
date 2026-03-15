#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include "SDL.h"
#include "sdl_pocket.h"
#include "opentyr.h"
#include "joystick.h"
#include "palette.h"
#include "opl.h"
#include "video.h"
#include "video_scale.h"

/* Hardware Register Definitions (SYS_CYCLE_LO/HI are in libc.h) */
#define SYSREG_BASE         0x40000000
#define SYS_CONT1_KEY       (*(volatile uint32_t *)(SYSREG_BASE + 0x50))
#define SYS_SHUTDOWN        (*(volatile uint32_t *)(SYSREG_BASE + 0x6C))

/* Audio Hardware (matches PocketDoom) */
#define AUDIO_SAMPLE        (*(volatile uint32_t *)0x4C000000)  /* Write: {L[15:0], R[15:0]} */
#define AUDIO_STATUS        (*(volatile uint32_t *)0x4C000004)  /* Read: [12]=full, [11:0]=level */
#define OPL2_ADDR           (*(volatile uint32_t *)0x4E000000)
#define OPL2_DATA           (*(volatile uint32_t *)0x4E000004)

#define FIFO_LEVEL(s)       ((s) & 0xFFF)
#define FIFO_FULL(s)        ((s) & (1 << 12))
#define FIFO_DEPTH          4096

#define CPU_HZ              100000000

/* External engine state */
extern Uint8 keysactive[SDL_NUM_SCANCODES];
extern Joystick *joystick;

/* ============================================
 * Engine Globals (HOME for surfaces)
 * ============================================ */

/* MUST define these exactly once in the entire project */
SDL_Surface *VGAScreen = NULL;
SDL_Surface *VGAScreenSeg = NULL;
SDL_Surface *VGAScreen2 = NULL;
SDL_Surface *game_screen = NULL;

SDL_Window *main_window = (SDL_Window *)1;
SDL_PixelFormat *main_window_tex_format = NULL;

int joysticks = 1;
bool joydown = false;
Joystick *joystick = NULL;
bool ignore_joystick = false;

int fullscreen_display = 0;
ScalingMode scaling_mode = SCALE_CENTER;
const char *const scaling_mode_names[ScalingMode_MAX] = { "Center", "Integer", "Fit 8:5", "Fit 4:3" };

uint scaler = 0;
const struct Scalers scalers[] = {{320, 200, NULL, NULL, "None"}};
const uint scalers_count = 1;

/* ============================================
 * Internal Atomic Timing
 * ============================================ */

static uint64_t get_hardware_cycles(void) {
    uint32_t lo, hi, hi2;
    do {
        hi = SYS_CYCLE_HI;
        lo = SYS_CYCLE_LO;
        hi2 = SYS_CYCLE_HI;
    } while (hi != hi2);
    return ((uint64_t)hi << 32) | lo;
}

/* ============================================
 * Event Ring Buffer
 * ============================================ */

#define EVENT_QUEUE_SIZE 64
static SDL_Event event_queue[EVENT_QUEUE_SIZE];
static int event_head = 0;
static int event_tail = 0;

static void push_event(SDL_Event *e) {
    int next = (event_head + 1) % EVENT_QUEUE_SIZE;
    if (next != event_tail) {
        event_queue[event_head] = *e;
        event_head = next;
    }
}

static uint32_t prev_buttons = 0;
static bool first_poll = true;

static const struct {
    uint32_t mask;
    SDL_Scancode scancode;
} button_map[] = {
    { (1 << 0),  SDL_SCANCODE_UP },
    { (1 << 1),  SDL_SCANCODE_DOWN },
    { (1 << 2),  SDL_SCANCODE_LEFT },
    { (1 << 3),  SDL_SCANCODE_RIGHT },
    { (1 << 4),  SDL_SCANCODE_LCTRL },
    { (1 << 5),  SDL_SCANCODE_LALT },
    { (1 << 6),  SDL_SCANCODE_SPACE },
    { (1 << 7),  SDL_SCANCODE_LSHIFT },
    { (1 << 8),  SDL_SCANCODE_TAB },
    { (1 << 9),  SDL_SCANCODE_RETURN },
    { (1 << 15), SDL_SCANCODE_RETURN },
    { (1 << 14), SDL_SCANCODE_ESCAPE }
};

void JE_poll_hardware_input(void) {
    uint32_t buttons = SYS_CONT1_KEY;
    if (first_poll) {
        prev_buttons = buttons;
        first_poll = false;
        return;
    }

    uint32_t changed = buttons ^ prev_buttons;
    if (changed == 0) return;

    for (int i = 0; i < 12; i++) {
        uint32_t m = button_map[i].mask;
        if (changed & m) {
            SDL_Event e;
            bool pressed = (buttons & m) != 0;
            e.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
            e.key.type = e.type;
            e.key.keysym.scancode = button_map[i].scancode;
            e.key.keysym.mod = 0;
            e.key.state = pressed ? SDL_PRESSED : SDL_RELEASED;
            e.key.repeat = 0;
            push_event(&e);
            
            if (e.key.keysym.scancode < SDL_NUM_SCANCODES) {
                keysactive[e.key.keysym.scancode] = pressed ? 1 : 0;
            }

            if (pressed) prev_buttons |= m;
            else prev_buttons &= ~m;
        }
    }
}

int SDL_PollEvent(SDL_Event *event) {
    JE_poll_hardware_input();
    if (event_tail == event_head) return 0;
    *event = event_queue[event_tail];
    event_tail = (event_tail + 1) % EVENT_QUEUE_SIZE;
    return 1;
}

/* ============================================
 * SDL Timing Shims
 * ============================================ */

Uint32 SDL_GetTicks(void) {
    if (SYS_SHUTDOWN & 1) {
        SYS_SHUTDOWN = 1; /* Ack */
        while (1);        /* Halt - hardware reset follows */
    }
    return (Uint32)(get_hardware_cycles() / (CPU_HZ / 1000));
}

void SDL_Delay(Uint32 ms) {
    Uint32 start = SDL_GetTicks();
    while (SDL_GetTicks() - start < ms) {
        service_audio();
    }
}

/* ============================================
 * Hardware OPL Driver (Adlib)
 * ============================================ */

void adlib_init(Bit32u samplerate) {
    (void)samplerate;
    for (int i = 1; i <= 0xF5; i++) {
        OPL2_ADDR = (uint32_t)(i & 0xFF);
        OPL2_DATA = 0;
    }
    OPL2_ADDR = 0x01;
    OPL2_DATA = 0x20;
}

void adlib_write(Bitu reg, Bit8u val) {
    OPL2_ADDR = (uint32_t)(reg & 0xFF);
    OPL2_DATA = (uint32_t)val;
}

void adlib_getsample(Bit16s* sndptr, Bits numsamples) {
    memset(sndptr, 0, numsamples * sizeof(Bit16s));
}

/* ============================================
 * SDL Audio Driver
 * ============================================ */

typedef struct {
    SDL_AudioSpec spec;
    bool paused;
} PocketAudio;

static PocketAudio p_audio = { .paused = true };
static int16_t audio_buffer[4096];

SDL_AudioDeviceID SDL_OpenAudioDevice(const char *d, int c, const SDL_AudioSpec *desired, SDL_AudioSpec *obtained, int a) {
    (void)d; (void)c; (void)a;
    p_audio.spec = *desired;
    /* Hardware is fixed at 48kHz stereo */
    p_audio.spec.freq = 48000;
    p_audio.spec.channels = 2; 
    if (obtained) {
        obtained->freq = 48000;
        obtained->channels = 2;
        obtained->format = AUDIO_S16SYS;
    }
    return 1;
}

void SDL_PauseAudioDevice(SDL_AudioDeviceID d, int p) { (void)d; p_audio.paused = (bool)p; }
void SDL_CloseAudioDevice(SDL_AudioDeviceID d) { (void)d; p_audio.spec.callback = NULL; }
void SDL_LockAudioDevice(SDL_AudioDeviceID d) { (void)d; }
void SDL_UnlockAudioDevice(SDL_AudioDeviceID d) { (void)d; }

struct audio_convert_params {
    SDL_AudioFormat src_format, dst_format;
    int src_rate, dst_rate;
    uint8_t src_channels, dst_channels;
};

static struct audio_convert_params current_cvt_params;

int SDL_BuildAudioCVT(SDL_AudioCVT *c, SDL_AudioFormat sf, uint8_t sc, int sr, SDL_AudioFormat df, uint8_t dc, int dr) {
    c->needed = (sf != df || sc != dc || sr != dr);
    c->src_format = sf;
    c->dst_format = df;
    
    int mult = 1;
    if (df == AUDIO_S16SYS && sf == AUDIO_S8) mult *= 2;
    mult *= (dc / sc);
    mult *= (dr + sr - 1) / sr;
    c->len_mult = mult;
    c->len_ratio = (double)dr / sr * (double)(df == AUDIO_S16SYS ? 2 : 1) / (sf == AUDIO_S16SYS ? 2 : 1) * (double)dc / sc;
    
    current_cvt_params.src_format = sf;
    current_cvt_params.dst_format = df;
    current_cvt_params.src_rate = sr;
    current_cvt_params.dst_rate = dr;
    current_cvt_params.src_channels = sc;
    current_cvt_params.dst_channels = dc;
    
    return 0;
}

int SDL_ConvertAudio(SDL_AudioCVT *c) {
    if (!c->needed) {
        c->len_cvt = c->len;
        return 0;
    }

    /* VERIFIED TESTED ALGORITHM: 8-bit signed -> 16-bit signed with multiplier  */
    if (current_cvt_params.src_format == AUDIO_S8 && current_cvt_params.dst_format == AUDIO_S16SYS) {
        int src_len = c->len;
        uint32_t src_rate = current_cvt_params.src_rate;
        uint32_t dst_rate = current_cvt_params.dst_rate;
        
        int dst_samples = (int)(((uint64_t)src_len * dst_rate) / src_rate);
        int dst_len = dst_samples * 2;
        
        int8_t *src = (int8_t *)c->buf;
        int16_t *dst = (int16_t *)c->buf;
        
        uint32_t step = (src_rate << 16) / dst_rate;

        for (int i = dst_samples - 1; i >= 0; i--) {
            uint32_t pos_fixed = i * step;
            uint32_t idx = pos_fixed >> 16;
            uint32_t frac = pos_fixed & 0xFFFF;
            
            if (idx >= (uint32_t)src_len - 1) {
                dst[i] = (int16_t)(src[src_len - 1] * 128);
            } else {
                int16_t s1 = (int16_t)(src[idx] * 128);
                int16_t s2 = (int16_t)(src[idx + 1] * 128);
                int32_t interpolated = s1 + (int32_t)(((int64_t)(s2 - s1) * (int32_t)frac) >> 16);
                dst[i] = (int16_t)interpolated;
            }
        }
        
        /* Anti-Aliasing Low-Pass Filter (4-pass FIR [1, 2, 1] / 4) */
        for (int p = 0; p < 4; p++) {
            int16_t prev = dst[0];
            for (int i = 1; i < dst_samples - 1; i++) {
                int16_t curr = dst[i];
                dst[i] = (int16_t)(((int32_t)prev + ((int32_t)curr << 1) + (int32_t)dst[i + 1]) >> 2);
                prev = curr;
            }
        }
        
        c->len_cvt = dst_len;
        return 0;
    }

    return -1;
}

void service_audio(void) {
    if (p_audio.paused || !p_audio.spec.callback) return;

    uint32_t status = AUDIO_STATUS;
    int buffered = FIFO_LEVEL(status);
    if (FIFO_FULL(status)) buffered = FIFO_DEPTH;
    
    int available = FIFO_DEPTH - buffered;

    if (available < 512) return;

    /* Process up to 1024 samples per call to avoid hanging the CPU.
     * Do NOT loop here; dual-clock FIFOs have a status update delay.
     * Returning allows the status to sync before the next call.
     */
    int to_render = (available > 1024) ? 1024 : available;

    p_audio.spec.callback(p_audio.spec.userdata, (Uint8 *)audio_buffer, to_render * sizeof(int16_t));

    for (int i = 0; i < to_render; i++) {
        int16_t s = audio_buffer[i];
        /* Interleaved Stereo: L in upper 16, R in lower 16 */
        AUDIO_SAMPLE = ((uint32_t)(uint16_t)s << 16) | (uint16_t)s;
    }
}

/* ============================================
 * Platform Shims
 * ============================================ */

char *getenv(const char *name) { (void)name; return NULL; }
struct tm *localtime(const time_t *t) { (void)t; static struct tm tm; memset(&tm, 0, sizeof(tm)); return &tm; }
int mkdir(const char *p, int mode) { (void)p; (void)mode; return 0; }

void deinit_video(void) {}
void deinit_joysticks(void) {}

void push_joysticks_as_keyboard(void) {}
void poll_joysticks(void) { JE_poll_hardware_input(); }
void poll_joystick(int j) { (void)j; JE_poll_hardware_input(); }
void reset_joystick_assignments(int j) { (void)j; }
bool detect_joystick_assignment(int j, Joystick_assignment *a) { (void)j; (void)a; return false; }
bool joystick_assignment_cmp(const Joystick_assignment *a, const Joystick_assignment *b) { (void)a; (void)b; return false; }
void joystick_assignments_to_string(char *b, size_t l, const Joystick_assignment *a) { (void)a; if(b)b[0]=0; }
int joystick_axis_reduce(int j, int v) { (void)j; return v; }
bool joystick_analog_angle(int j, float *a) { (void)j; return false; }

void init_joysticks(void) {
    if (!joystick) joystick = calloc(1, sizeof(Joystick));
}

void reinit_fullscreen(int d) { (void)d; }
bool init_scaler(unsigned int s) { (void)s; return true; }
void video_on_win_resize(void) {}
void toggle_fullscreen(void) {}

void mapWindowPointToScreen(Sint32 *x, Sint32 *y) { (void)x; (void)y; }
void scaleWindowDistanceToScreen(Sint32 *x, Sint32 *y) { (void)x; (void)y; }

bool set_scaling_mode_by_name(const char *name) { (void)name; return true; }
void set_scaler_by_name(const char *name) { (void)name; }

int SDL_Init(Uint32 f) { (void)f; return 0; }
int SDL_InitSubSystem(Uint32 f) { (void)f; return 0; }
Uint32 SDL_WasInit(Uint32 f) { (void)f; return 1; }
void SDL_Quit(void) {}
void SDL_QuitSubSystem(Uint32 f) { (void)f; }
const char *SDL_GetError(void) { return "Success"; }
SDL_Window *SDL_CreateWindow(const char *t, int x, int y, int w, int h, Uint32 f) { (void)t; return (SDL_Window *)1; }
void SDL_GetWindowSize(SDL_Window *w, int *w1, int *h) { if(w1)*w1=320; if(h)*h=240; }
void SDL_FreeSurface(SDL_Surface *s) {
    if (s) {
        uint32_t addr = (uint32_t)s->pixels;
        if (addr < 0x10000000 || addr >= 0x14000000) free(s->pixels);
        if (s->format) {
            if (s->format->palette) {
                if (s->format->palette->colors) free(s->format->palette->colors);
                free(s->format->palette);
            }
            free(s->format);
        }
        free(s);
    }
}
int SDL_FillRect(SDL_Surface *dst, const SDL_Rect *rect, Uint32 color) {
    if (!dst || !dst->pixels) return -1;
    int x = 0, y = 0, w = dst->w, h = dst->h;
    if (rect) {
        x = rect->x; y = rect->y; w = rect->w; h = rect->h;
    }
    // Clip
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > dst->w) w = dst->w - x;
    if (y + h > dst->h) h = dst->h - y;
    if (w <= 0 || h <= 0) return 0;
    
    uint8_t *p = (uint8_t *)dst->pixels + y * dst->pitch + x;
    for (int i = 0; i < h; i++) {
        memset(p, color, w);
        p += dst->pitch;
    }
    return 0;
}
void JE_clr256(SDL_Surface *s) { if (s && s->pixels) memset(s->pixels, 0, s->w * s->h); }
Uint32 SDL_MapRGB(const SDL_PixelFormat *f, Uint8 r, Uint8 g, Uint8 b) { return 0; }
int SDL_GetNumVideoDisplays(void) { return 1; }
SDL_Scancode SDL_GetScancodeFromName(const char *n) { return 0; }
const char *SDL_GetScancodeName(SDL_Scancode s) { return ""; }
SDL_Keymod SDL_GetModState(void) { return 0; }
void SDL_SetRelativeMouseMode(SDL_bool enable) { (void)enable; }
int SDL_ShowCursor(int toggle) { return 0; }

size_t SDL_strlcpy(char *d, const char *s, size_t sz) {
    size_t l = strlen(s);
    if (sz > 0) {
        size_t n = (l >= sz) ? sz - 1 : l;
        memcpy(d, s, n);
        d[n] = '\0';
    }
    return l;
}
