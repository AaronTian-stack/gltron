#include "audio/nebu_audio_system.h"

#include "base/nebu_system.h"
#include <stdio.h>

void nebu_Audio_Init(void)
{
	if(!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO)) {
		if(!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
			fprintf(stderr, "Couldn't initialize SDL audio: %s\n", SDL_GetError());
			/* FIXME: disable sound system */
		}
	}
}

