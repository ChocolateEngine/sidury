#pragma once

// 
// The Server, only runs if the engine is a dedicated server, or hosting on the client
// 

bool SV_Init();
void SV_Shutdown();
void SV_Update( float frameTime );

