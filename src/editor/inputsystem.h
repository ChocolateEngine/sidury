#pragma once

#include "iinput.h"
#include <string>

enum EBinding : u16
{
	// General
	EBinding_General_Undo,
	EBinding_General_Redo,
	EBinding_General_Cut,
	EBinding_General_Copy,
	EBinding_General_Paste,

	// Viewport Binds
	EBinding_Viewport_MouseLook,
	EBinding_Viewport_MoveForward,
	EBinding_Viewport_MoveBack,
	EBinding_Viewport_MoveLeft,
	EBinding_Viewport_MoveRight,
	EBinding_Viewport_MoveUp,
	EBinding_Viewport_MoveDown,
	EBinding_Viewport_Sprint,
	EBinding_Viewport_Slow,
	EBinding_Viewport_Select,
	EBinding_Viewport_IncreaseMoveSpeed,
	EBinding_Viewport_DecreaseMoveSpeed,

	EBinding_Count,
};


enum EBindingType	
{
	EBindingType_Hold,
	EBindingType_Toggle,
	EBindingType_RepeatHold,
	EBindingType_RepeatToggle,
};


using InputContextHandle_t = int;
using KeyBindCategory_t = u32;


// Properties for the current input
struct InputContext_t
{
	bool aCapturesMouse    = false;
	bool aCapturesKeyboard = false;
};


void                 Input_Init();
void                 Input_Update();

void                 Input_CalcMouseDelta();
glm::vec2            Input_GetMouseDelta();

// could use some stack system to determine who gets to control the mouse scale maybe
void                 Input_SetMouseDeltaScale( const glm::vec2& scale );
const glm::vec2&     Input_GetMouseDeltaScale();

InputContextHandle_t Input_CreateContext( InputContext_t sContext );
void                 Input_FreeContext( InputContextHandle_t sContextHandle );

InputContext_t&      Input_GetContextData( InputContext_t sContext );

// Push a context to the top of the stack, this will be the currently enabled context
// Returns true if the context was successfully pushed to the top of the stack
bool                 Input_PushContext( InputContextHandle_t sContextHandle );
void                 Input_PopContext();

// Is the currently enabled context the enabled one?
bool                 Input_IsCurrentContext( InputContextHandle_t sContextHandle );

const char*          Input_BindingToStr( EBinding sBinding );

// TODO: key combinations, hmm
void                 Input_BindKey( EButton sKey, EBinding sKeyBind );
void                 Input_BindKey( SDL_Scancode sKey, EBinding sKeyBind );

void                 Input_BindKeys( EButton* spKeys, u8 sKeyCount, EBinding sKeyBind );
void                 Input_BindKeys( SDL_Scancode* spKeys, u8 sKeyCount, EBinding sKeyBind );

void                 Input_BindKeys( std::vector< EButton > sKeys, EBinding sKeyBind );
void                 Input_BindKeys( std::vector< SDL_Scancode > sKeys, EBinding sKeyBind );

void                 Input_ResetBinds();

bool                 Input_KeyPressed( EBinding sKeyBind );
bool                 Input_KeyReleased( EBinding sKeyBind );

bool                 Input_KeyJustPressed( EBinding sKeyBind );
bool                 Input_KeyJustReleased( EBinding sKeyBind );

KeyState             Input_KeyState( EBinding sKeyBind );

KeyBindCategory_t    Input_CreateBindCategory( std::string_view sName );
