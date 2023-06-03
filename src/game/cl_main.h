#pragma once

//
// The Client, always running unless a dedicated server
// 


enum EClientState
{
	EClientState_Idle,
	EClientState_RecvServerInfo,
	EClientState_Connecting,
	EClientState_Connected,
	EClientState_Count
};


struct CL_ServerData_t
{
	std::string aName;
	std::string aMapName;
	int         aPlayerCount;
};


bool CL_Init();
void CL_Shutdown();
void CL_Update( float frameTime );

void CL_Connect( const char* spAddress );
void CL_Disconnect( const char* spReason = nullptr );

void CL_GameUpdate( float frameTime );
void CL_UpdateUserCmd();
void CL_SendUserCmd();
void CL_GetServerMessages();
