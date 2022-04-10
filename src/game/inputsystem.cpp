#include "gamesystem.h"
#include "inputsystem.h"

GameInput gameinput;


CONVAR( m_pitch, 0.022 );
CONVAR( m_yaw, 0.022 );
CONVAR( m_sensitivity, 0.3 );


void GameInput::Init()
{
	CalcMouseDelta();
}


void GameInput::Update()
{
	aMouseDelta = {};

	CalcMouseDelta();
}


void GameInput::CalcMouseDelta()
{
	const glm::ivec2& baseDelta = input->GetMouseDelta();

	aMouseDelta.x = baseDelta.x * m_sensitivity;
	aMouseDelta.y = baseDelta.y * m_sensitivity;
}


const glm::vec2& GameInput::GetMouseDelta()
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

