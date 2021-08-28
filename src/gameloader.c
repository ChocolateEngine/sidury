#include "../chocolate/inc/shared/platform.h"
#include <stdio.h>

int main
	(  )
{
	Module handle;
	void ( *engine_start )(  );

// bruh
#ifdef _WIN32
	handle = LOAD_LIBRARY( "bin/engine.dll" );
#elif __linux__
	handle = LOAD_LIBRARY( "bin/engine.so" );
#endif

	if ( !handle )
	{
		// fprintf( stderr, "Error: %s\n", GET_ERROR(  ) );
		PrintLastError( "Failed to load engine" );
		return -1;
	}

	*( void** )( &engine_start ) = LOAD_FUNC( handle, "engine_start" );
	if ( !engine_start )
	{
		fprintf( stderr, "Error: %s\n", GET_ERROR(  ) );
		CLOSE_LIBRARY( handle );
		return -1;
	}

	engine_start(  );
	CLOSE_LIBRARY( handle );

	return 0;
}
