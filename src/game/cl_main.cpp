#include "cl_main.h"

//
// The Client, always running unless a dedicated server
// 


CONVAR( cl_username, "Player", CVARF_ARCHIVE );


bool CL_Init()
{
	return true;
}


void CL_Shutdown()
{
}


void CL_Update( float frameTime )
{
}

