#pragma once

//
// The Client, always running unless a dedicated server
// 


struct UserCmd_t;


extern UserCmd_t gClientUserCmd;


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


extern CL_ServerData_t gClientServerData;


bool CL_Init();
void CL_Shutdown();
void CL_Update( float frameTime );
void CL_GameUpdate( float frameTime );

bool CL_IsMenuShown();
void CL_UpdateMenuShown();

void CL_Connect( const char* spAddress );
void CL_Disconnect( bool sSendReason = true, const char* spReason = nullptr );

int  CL_WriteToServer( capnp::MessageBuilder& srBuilder );

bool CL_RecvServerInfo();
void CL_UpdateUserCmd();
void CL_SendUserCmd( capnp::MessageBuilder& srBuilder );
void CL_GetServerMessages();
