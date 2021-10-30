#pragma once

#include <SDL2/SDL_scancode.h>
#include <string>


enum ButtonInputs_
{
	In_None = 0,

	In_Forward = (1 << 1),
	In_Back = (1 << 2),
	In_Right = 4,
	In_Left = 8,

	In_Sprint = 16,
	//In_Walk = 32, // maybe?
	In_Duck = 32,

	// hmm, idk, this would work here though
	In_Noclip = 64,


};

typedef unsigned int ButtonInput_t;


// set this up later
class GameInput
{
	//void BindKey( SDL_Scancode key, ButtonInput_t key );

	//bool KeyPressed( ButtonInput_t key );
	//bool KeyReleased( ButtonInput_t key );
	//bool KeyJustPressed( ButtonInput_t key );
	//bool KeyJustReleased( ButtonInput_t key );
};

// extern GameInput* gameinput;

