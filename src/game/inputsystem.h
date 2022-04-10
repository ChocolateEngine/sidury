#pragma once

#include <SDL2/SDL_scancode.h>
#include <string>


enum ButtonInputs_
{
	In_None = 0,

	In_Forward = (1 << 1),
	In_Back = (1 << 2),
	In_Right = (1 << 3),
	In_Left = (1 << 4),

	In_Sprint = (1 << 5),
	//In_Walk = 32, // maybe?
	In_Duck = (1 << 6),
	In_Zoom = (1 << 7),

	// hmm, idk, this would work here though
	In_Noclip = (1 << 8),


};

typedef unsigned int ButtonInput_t;


// only handles mouse input right now
// also make component based maybe in-case of a controller, 
// we don't need any mouse sens or key input calculations here
// maybe a list of IGameInputDevice?
class GameInput
{
public:

	void                Init();
	void                Update();

	void                CalcMouseDelta();
	const glm::vec2&    GetMouseDelta();

	// could use some stack system to determine who gets to control the mouse scale maybe
	void                SetMouseDeltaScale( const glm::vec2& scale );
	const glm::vec2&    GetMouseDeltaScale();

	//void BindKey( SDL_Scancode key, ButtonInput_t key );

	//bool KeyPressed( ButtonInput_t key );
	//bool KeyReleased( ButtonInput_t key );
	//bool KeyJustPressed( ButtonInput_t key );
	//bool KeyJustReleased( ButtonInput_t key );

	glm::vec2 aMouseDelta{};
	glm::vec2 aMouseDeltaScale{1.f, 1.f};
};

extern GameInput gameinput;

