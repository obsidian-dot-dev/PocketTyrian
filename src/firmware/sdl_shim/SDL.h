#ifndef SDL_H
#define SDL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef int8_t Sint8;
typedef uint8_t Uint8;
typedef int16_t Sint16;
typedef uint16_t Uint16;
typedef int32_t Sint32;
typedef uint32_t Uint32;
typedef int64_t Sint64;
typedef uint64_t Uint64;

typedef enum {
    SDL_FALSE = 0,
    SDL_TRUE = 1
} SDL_bool;

#define SDL_PRESSED 1
#define SDL_RELEASED 0

#define SDL_INIT_VIDEO    0x00000020u
#define SDL_INIT_JOYSTICK 0x00000200u
#define SDL_INIT_AUDIO    0x00000010u

typedef struct { int x, y, w, h; } SDL_Rect;
typedef struct { Uint8 r, g, b, a; } SDL_Color;

typedef struct {
    int ncolors;
    SDL_Color *colors;
} SDL_Palette;

typedef struct {
    Uint32 format;
    SDL_Palette *palette;
    Uint8 BitsPerPixel;
    Uint8 BytesPerPixel;
    Uint32 Rmask, Gmask, Bmask, Amask;
} SDL_PixelFormat;

typedef struct {
    SDL_PixelFormat *format;
    int w, h;
    int pitch;
    void *pixels;
    void *userdata;
} SDL_Surface;

typedef enum {
    SDL_SCANCODE_UNKNOWN = 0,
    SDL_SCANCODE_A = 4, SDL_SCANCODE_B = 5, SDL_SCANCODE_C = 6, SDL_SCANCODE_D = 7,
    SDL_SCANCODE_E = 8, SDL_SCANCODE_F = 9, SDL_SCANCODE_G = 10, SDL_SCANCODE_H = 11,
    SDL_SCANCODE_I = 12, SDL_SCANCODE_J = 13, SDL_SCANCODE_K = 14, SDL_SCANCODE_L = 15,
    SDL_SCANCODE_M = 16, SDL_SCANCODE_N = 17, SDL_SCANCODE_O = 18, SDL_SCANCODE_P = 19,
    SDL_SCANCODE_Q = 20, SDL_SCANCODE_R = 21, SDL_SCANCODE_S = 22, SDL_SCANCODE_T = 23,
    SDL_SCANCODE_U = 24, SDL_SCANCODE_V = 25, SDL_SCANCODE_W = 26, SDL_SCANCODE_X = 27,
    SDL_SCANCODE_Y = 28, SDL_SCANCODE_Z = 29,
    SDL_SCANCODE_1 = 30, SDL_SCANCODE_2 = 31, SDL_SCANCODE_3 = 32, SDL_SCANCODE_4 = 33,
    SDL_SCANCODE_5 = 34, SDL_SCANCODE_6 = 35, SDL_SCANCODE_7 = 36, SDL_SCANCODE_8 = 37,
    SDL_SCANCODE_9 = 38, SDL_SCANCODE_0 = 39,
    SDL_SCANCODE_RETURN = 40, SDL_SCANCODE_ESCAPE = 41, SDL_SCANCODE_BACKSPACE = 42,
    SDL_SCANCODE_TAB = 43, SDL_SCANCODE_SPACE = 44,
    SDL_SCANCODE_MINUS = 45, SDL_SCANCODE_EQUALS = 46, SDL_SCANCODE_LEFTBRACKET = 47,
    SDL_SCANCODE_RIGHTBRACKET = 48, SDL_SCANCODE_BACKSLASH = 49,
    SDL_SCANCODE_SEMICOLON = 51, SDL_SCANCODE_APOSTROPHE = 52, SDL_SCANCODE_GRAVE = 53,
    SDL_SCANCODE_COMMA = 54, SDL_SCANCODE_PERIOD = 55, SDL_SCANCODE_SLASH = 56,
    SDL_SCANCODE_CAPSLOCK = 57,
    SDL_SCANCODE_PRINTSCREEN = 70,
    SDL_SCANCODE_SCROLLLOCK = 71,
    SDL_SCANCODE_PAUSE = 72,
    SDL_SCANCODE_F1 = 58, SDL_SCANCODE_F2 = 59, SDL_SCANCODE_F3 = 60, SDL_SCANCODE_F4 = 61,
    SDL_SCANCODE_F5 = 62, SDL_SCANCODE_F6 = 63, SDL_SCANCODE_F7 = 64, SDL_SCANCODE_F8 = 65,
    SDL_SCANCODE_F9 = 66, SDL_SCANCODE_F10 = 67, SDL_SCANCODE_F11 = 68, SDL_SCANCODE_F12 = 69,
    SDL_SCANCODE_INSERT = 73, SDL_SCANCODE_HOME = 74, SDL_SCANCODE_PAGEUP = 75,
    SDL_SCANCODE_DELETE = 76, SDL_SCANCODE_END = 77, SDL_SCANCODE_PAGEDOWN = 78,
    SDL_SCANCODE_RIGHT = 79, SDL_SCANCODE_LEFT = 80, SDL_SCANCODE_DOWN = 81, SDL_SCANCODE_UP = 82,
    SDL_SCANCODE_NUMLOCKCLEAR = 83, SDL_SCANCODE_KP_DIVIDE = 84, SDL_SCANCODE_KP_MULTIPLY = 85,
    SDL_SCANCODE_KP_MINUS = 86, SDL_SCANCODE_KP_PLUS = 87, SDL_SCANCODE_KP_ENTER = 88,
    SDL_SCANCODE_KP_1 = 89, SDL_SCANCODE_KP_2 = 90, SDL_SCANCODE_KP_3 = 91,
    SDL_SCANCODE_KP_4 = 92, SDL_SCANCODE_KP_5 = 93, SDL_SCANCODE_KP_6 = 94,
    SDL_SCANCODE_KP_7 = 95, SDL_SCANCODE_KP_8 = 96, SDL_SCANCODE_KP_9 = 97,
    SDL_SCANCODE_KP_0 = 98, SDL_SCANCODE_KP_PERIOD = 99,
    SDL_SCANCODE_LCTRL = 224, SDL_SCANCODE_LSHIFT = 225, SDL_SCANCODE_LALT = 226,
    SDL_SCANCODE_RCTRL = 228, SDL_SCANCODE_RSHIFT = 229, SDL_SCANCODE_RALT = 230,
    SDL_NUM_SCANCODES = 512
} SDL_Scancode;

typedef enum {
    KMOD_NONE = 0x0000, KMOD_LSHIFT = 0x0001, KMOD_RSHIFT = 0x0002,
    KMOD_LCTRL = 0x0040, KMOD_RCTRL = 0x0080, KMOD_LALT = 0x0100, KMOD_RALT = 0x0200,
    KMOD_GUI = 0x0400, KMOD_CTRL = KMOD_LCTRL | KMOD_RCTRL,
    KMOD_SHIFT = KMOD_LSHIFT | KMOD_RSHIFT, KMOD_ALT = KMOD_LALT | KMOD_RALT
} SDL_Keymod;

typedef struct { SDL_Scancode scancode; SDL_Keymod mod; } SDL_Keysym;
typedef struct { Uint8 type; Uint32 timestamp; Uint32 windowID; Uint8 state; Uint8 repeat; SDL_Keysym keysym; } SDL_KeyboardEvent;
typedef struct { Uint8 type; Uint32 timestamp; Uint32 windowID; Uint32 which; Uint32 state; Sint32 x, y, xrel, yrel; } SDL_MouseMotionEvent;
typedef struct { Uint8 type; Uint32 timestamp; Uint32 windowID; Uint32 which; Uint8 button, state, clicks; Sint32 x, y; } SDL_MouseButtonEvent;
typedef struct { Uint8 type; Uint32 timestamp; char text[32]; } SDL_TextInputEvent;
#define SDL_TEXTINPUTEVENT_TEXT_SIZE 32

typedef enum {
    SDL_WINDOWEVENT_NONE, SDL_WINDOWEVENT_SHOWN, SDL_WINDOWEVENT_HIDDEN, SDL_WINDOWEVENT_EXPOSED,
    SDL_WINDOWEVENT_MOVED, SDL_WINDOWEVENT_RESIZED, SDL_WINDOWEVENT_SIZE_CHANGED,
    SDL_WINDOWEVENT_MINIMIZED, SDL_WINDOWEVENT_MAXIMIZED, SDL_WINDOWEVENT_RESTORED,
    SDL_WINDOWEVENT_ENTER, SDL_WINDOWEVENT_LEAVE, SDL_WINDOWEVENT_FOCUS_GAINED,
    SDL_WINDOWEVENT_FOCUS_LOST, SDL_WINDOWEVENT_CLOSE, SDL_WINDOWEVENT_TAKE_FOCUS,
    SDL_WINDOWEVENT_HIT_TEST
} SDL_WindowEventID;

typedef struct { Uint32 type; Uint32 timestamp; Uint32 windowID; Uint8 event; Sint32 data1, data2; } SDL_WindowEvent;

typedef union {
    Uint32 type; SDL_KeyboardEvent key; SDL_MouseMotionEvent motion;
    SDL_MouseButtonEvent button; SDL_TextInputEvent text; SDL_WindowEvent window;
} SDL_Event;

#define SDL_KEYDOWN 0x300
#define SDL_KEYUP   0x301
#define SDL_MOUSEMOTION 0x400
#define SDL_MOUSEBUTTONDOWN 0x401
#define SDL_MOUSEBUTTONUP 0x402
#define SDL_QUIT 0x100
#define SDL_TEXTINPUT 0x700
#define SDL_WINDOWEVENT 0x200
#define SDL_TEXTEDITING 0x701

#define SDL_WINDOW_HIDDEN 0x00000008
#define SDL_WINDOW_RESIZABLE 0x00000020
#define SDL_WINDOWPOS_CENTERED 0x2FFF0000u

#define SDL_BUTTON_LEFT     1
#define SDL_BUTTON_MIDDLE   2
#define SDL_BUTTON_RIGHT    3

typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;
int SDL_Init(Uint32 f);
int SDL_InitSubSystem(Uint32 f);
Uint32 SDL_WasInit(Uint32 f);
void SDL_Quit(void);
void SDL_QuitSubSystem(Uint32 f);
const char *SDL_GetError(void);
SDL_Window *SDL_CreateWindow(const char *t, int x, int y, int w, int h, Uint32 f);
void SDL_GetWindowSize(SDL_Window *w, int *w1, int *h);
SDL_Surface *SDL_CreateRGBSurface(Uint32 f, int w, int h, int d, Uint32 rm, Uint32 gm, Uint32 bm, Uint32 am);
void SDL_FreeSurface(SDL_Surface *s);
int SDL_FillRect(SDL_Surface *d, const SDL_Rect *r, Uint32 c);
Uint32 SDL_MapRGB(const SDL_PixelFormat *f, Uint8 r, Uint8 g, Uint8 b);
int SDL_PollEvent(SDL_Event *e);
Uint32 SDL_GetTicks(void);
void SDL_Delay(Uint32 ms);
int SDL_ShowCursor(int t);
SDL_Scancode SDL_GetScancodeFromName(const char *n);
const char *SDL_GetScancodeName(SDL_Scancode s);
SDL_Keymod SDL_GetModState(void);
size_t SDL_strlcpy(char *d, const char *s, size_t sz);
int SDL_GetNumVideoDisplays(void);
void SDL_SetRelativeMouseMode(SDL_bool enable);

typedef uint32_t SDL_AudioDeviceID;
typedef uint16_t SDL_AudioFormat;
#define AUDIO_S8     0x8008
#define AUDIO_S16SYS 0x8010
typedef void (*SDL_AudioCallback)(void *u, Uint8 *s, int l);
typedef struct { int freq; SDL_AudioFormat format; Uint8 channels, silence; Uint16 samples; Uint32 size; SDL_AudioCallback callback; void *userdata; } SDL_AudioSpec;
typedef struct SDL_AudioCVT SDL_AudioCVT;
struct SDL_AudioCVT { int needed; SDL_AudioFormat src_format, dst_format; double rate_incr; Uint8 *buf; int len, len_cvt, len_mult; double len_ratio; void (*filters[10])(SDL_AudioCVT *c, SDL_AudioFormat f); int filter_index; };
SDL_AudioDeviceID SDL_OpenAudioDevice(const char *d, int c, const SDL_AudioSpec *de, SDL_AudioSpec *o, int a);
void SDL_PauseAudioDevice(SDL_AudioDeviceID d, int p);
void SDL_CloseAudioDevice(SDL_AudioDeviceID d);
void SDL_LockAudioDevice(SDL_AudioDeviceID d);
void SDL_UnlockAudioDevice(SDL_AudioDeviceID d);
int SDL_BuildAudioCVT(SDL_AudioCVT *c, SDL_AudioFormat sf, Uint8 sc, int sr, SDL_AudioFormat df, Uint8 dc, int dr);
int SDL_ConvertAudio(SDL_AudioCVT *c);

typedef int SDL_JoystickID;
typedef struct _SDL_Joystick SDL_Joystick;

#define SDL_VERSION_ATLEAST(X, Y, Z) (0)

#define SDL_AUDIO_ALLOW_FREQUENCY_CHANGE 0x00000001
#define SDL_AUDIO_ALLOW_FORMAT_CHANGE    0x00000002
#define SDL_AUDIO_ALLOW_CHANNELS_CHANGE  0x00000004
#define SDL_AUDIO_ALLOW_SAMPLES_CHANGE   0x00000008

#endif
