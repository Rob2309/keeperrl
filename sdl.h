#pragma once

#include "stdafx.h"

namespace SDL {
#include <SDL2/SDL.h>
#include <SDL_image.h>
}

#undef TECHNOLOGY
#undef TRANSPARENT

using SDL::Uint8;

typedef SDL::SDL_Event Event;
typedef SDL::SDL_EventType EventType;
