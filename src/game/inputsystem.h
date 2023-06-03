#pragma once

#include <SDL2/SDL_scancode.h>
#include <string>

extern ConVarFlag_t CVARF_INPUT;

using ButtonInput_t = int;

enum EBtnInput
{
	EBtnInput_None    = 0,

	EBtnInput_Forward = ( 1 << 1 ),
	EBtnInput_Back    = ( 1 << 2 ),
	EBtnInput_Right   = ( 1 << 3 ),
	EBtnInput_Left    = ( 1 << 4 ),

	EBtnInput_Sprint  = ( 1 << 5 ),
	EBtnInput_Duck    = ( 1 << 6 ),
	EBtnInput_Jump    = ( 1 << 7 ),
	EBtnInput_Zoom    = ( 1 << 8 ),
};


constexpr float      IN_CVAR_JUST_RELEASED = -1.f;
constexpr float      IN_CVAR_RELEASED      = 0.f;
constexpr float      IN_CVAR_PRESSED       = 1.f;
constexpr float      IN_CVAR_JUST_PRESSED  = 2.f;


void                 Input_Init();
void                 Input_Update();

void                 Input_CalcMouseDelta();
glm::vec2            Input_GetMouseDelta();

// could use some stack system to determine who gets to control the mouse scale maybe
void                 Input_SetMouseDeltaScale( const glm::vec2& scale );
const glm::vec2&     Input_GetMouseDeltaScale();

// Button Handling
ButtonInput_t        Input_RegisterButton();
ButtonInput_t        Input_GetButtonStates();

void                 Input_BindKey( SDL_Scancode key, const std::string& cmd );

//bool Input_KeyPressed( ButtonInput_t key );
//bool Input_KeyReleased( ButtonInput_t key );
//bool Input_KeyJustPressed( ButtonInput_t key );
//bool Input_KeyJustReleased( ButtonInput_t key );


extern ButtonInput_t IN_FORWARD;


