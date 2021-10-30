#include "../../src/game/gamesystem.h"
#include "core/platform.h"

#include <vector>

extern "C"
{
	void DLL_EXPORT game_init
		( std::vector< BaseSystem* > &systems )
	{
		systems.push_back( new GameSystem );
	}
}
