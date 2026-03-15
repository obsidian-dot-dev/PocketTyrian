#ifndef SDL_POCKET_H
#define SDL_POCKET_H

#include "SDL.h"

/* Samples hardware registers and pushes events into the ring buffer */
void JE_poll_hardware_input(void);

/* Processes audio callback and fills hardware FIFO */
void service_audio(void);

#endif
