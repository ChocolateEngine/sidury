#pragma once

//
// The Client, always running unless a dedicated server
// 

bool CL_Init();
void CL_Shutdown();
void CL_Update( float frameTime );

void CL_Disconnect();

