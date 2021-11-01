#pragma once

#include "shimdefs.h"

#include "graphics/imaterialsystem.h"
#include <SDL2/SDL.h>
#include "graphics/igraphics.h"

DLLOPEN( graphics );

DLLEXPORT(
	DLLFUNC( void, LoadModel, const char*, const char*, Model* )
	DLLFUNC( SDL_Window*, GetWindow )
	DLLFUNC( void, SetView, View* )
	DLLFUNC( void, GetWindowSize, uint32_t*, uint32_t* )
	DLLFUNC( void, DrawFrame )
	DLLFUNC( void, Init )
	DLLFUNC( IMaterialSystem*, GetMaterialSystem )
	DLLFUNC( Model*,  CreateModel )
	
	/* Gui functions.  */
	DLLFUNC( bool, IsConsoleShown )
	DLLFUNC( void, DrawGui )
	DLLFUNC( void, ShowConsole )
	DLLFUNC( void, DebugMessage, size_t, const char*, ... )
        DLLFUNC( void, InsertDebugMessage, size_t, const char*, ... )
	);

DLLOPEN( aduio );

DLLEXPORT(
	DLLFUNC( void, AduioInit )
	DLLFUNC( bool, LoadSound, const char*, AudioStream** )
	DLLFUNC( bool, PlaySound, AudioStream* )
	DLLFUNC( void, FreeSound, AudioStream** )
	DLLFUNC( void, AduioUpdate, float )
	DLLFUNC( void, SetListenerTransform, const glm::vec3*, const glm::vec3* )
	DLLFUNC( void, SetPaused, bool )
	);
	
DLLOPEN( engine );

DLLEXPORT(
	DLLFUNC( void, InitInput )
	DLLFUNC( void, InputUpdate, float )
	DLLFUNC( std::vector< SDL_Event >*, GetEvents )
	DLLFUNC( void, RegisterKey, SDL_Scancode )
	DLLFUNC( const glm::ivec2&, GetMouseDelta )
	DLLFUNC( bool, WindowHasFocus )
	DLLFUNC( bool, KeyPressed, SDL_Scancode )
	DLLFUNC( bool, KeyReleased, SDL_Scancode )
	DLLFUNC( bool, KeyJustPressed, SDL_Scancode )
	DLLFUNC( bool, KeyJustReleased, SDL_Scancode )
	);
