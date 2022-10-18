#include "inputsystem.h"
#include "iinput.h"

extern BaseInputSystem* input;

GameInput gameinput;

GameInput& GetGameInput()
{
	return gameinput;
}

#define REGISTER_BUTTON( name ) ButtonInput_t name = GetGameInput().RegisterButton()

REGISTER_BUTTON( IN_FORWARD );
REGISTER_BUTTON( IN_BACK );


CONVAR( m_pitch, 0.022 );
CONVAR( m_yaw, 0.022 );
CONVAR( m_sensitivity, 0.3, CVARF_ARCHIVE, "Mouse Sensitivity" );

extern ConVar in_forward;


void GameInput::Init()
{
	CalcMouseDelta();
}


void GameInput::Update()
{
	// update mouse inputs
	aMouseDelta = {};
	CalcMouseDelta();

	// update button binds and run the commands they are bound to
	
	// and for updating button states, do something like this
	aButtons = 0;

	if ( in_forward == 1.f )
		aButtons |= IN_FORWARD;

	else if ( in_forward == -1.f )
		aButtons |= IN_BACK;

}


void GameInput::CalcMouseDelta()
{
	const glm::ivec2& baseDelta = input->GetMouseDelta();

	aMouseDelta.x = baseDelta.x * m_sensitivity;
	aMouseDelta.y = baseDelta.y * m_sensitivity;
}


glm::vec2 GameInput::GetMouseDelta()
{
	return aMouseDelta * aMouseDeltaScale;
}


void GameInput::SetMouseDeltaScale( const glm::vec2& scale )
{
	aMouseDeltaScale = scale;
}

const glm::vec2& GameInput::GetMouseDeltaScale()
{
	return aMouseDeltaScale;
}


ButtonInput_t GameInput::RegisterButton()
{
	ButtonInput_t newBitShift = (1 << aButtonInputs.size());
	aButtonInputs.push_back( newBitShift );
	return newBitShift;
}


void GameInput::BindKey( SDL_Scancode key, const std::string& cmd )
{
	auto it = aKeyBinds.find( key );
	if ( it == aKeyBinds.end() )
	{
		// bind it
		aKeyBinds[key] = cmd;
	}
	else
	{
		// update this bind (does this work?)
		it->second = cmd;
	}
}

