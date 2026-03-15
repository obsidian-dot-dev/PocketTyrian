#ifndef SDL_ENDIAN_H
#define SDL_ENDIAN_H
#include "SDL_types.h"
#define SDL_LIL_ENDIAN 1234
#define SDL_BIG_ENDIAN 4321
#define SDL_BYTEORDER SDL_LIL_ENDIAN
static inline Uint16 SDL_Swap16(Uint16 x) { return (x << 8) | (x >> 8); }
static inline Uint32 SDL_Swap32(Uint32 x) { return ((x << 24) | ((x << 8) & 0x00FF0000) | ((x >> 8) & 0x0000FF00) | (x >> 24)); }
#define SDL_SwapLE16(X) (X)
#define SDL_SwapLE32(X) (X)
#define SDL_SwapBE16(X) SDL_Swap16(X)
#define SDL_SwapBE32(X) SDL_Swap32(X)
#endif
