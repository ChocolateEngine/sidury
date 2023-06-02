#pragma once

#include "network/net_main.h"
#include "capnproto/sidury.capnp.h"

// 
// The Server, only runs if the engine is a dedicated server, or hosting on the client
// 

EXT_CVAR_FLAG( CVARF_SERVER );


enum ESVClientState
{
	ESV_ClientState_Connecting,
	ESV_ClientState_Connected,
	ESV_ClientState_Disconnected,
};


struct SV_Client_t
{
	Socket_t       aSocket;
	ch_sockaddr    aAddr;

	std::string    aName;
	ESVClientState aState;
};


struct ServerData_t
{
	bool                       aActive;
	std::vector< SV_Client_t > aClients;
	std::vector< SV_Client_t* > aClientsConnecting;
};

bool                SV_Init();
void                SV_Shutdown();
void                SV_Update( float frameTime );

void                SV_GameUpdate( float frameTime );
void                SV_BuildUpdatedData( capnp::MessageBuilder& srMessage );

void                SV_ProcessClient( SV_Client_t& srClient );
void                SV_CheckForNewClients();
void                SV_ConnectClientFinish( SV_Client_t& srClient );

void                SV_SendMessageToClient( SV_Client_t& srClient, capnp::MessageBuilder& srMessage );
void                SV_SendServerInfo( SV_Client_t& srClient );
void                SV_SendDisconnect( SV_Client_t& srClient );

void                SV_SetCommandClient( SV_Client_t* spClient );
SV_Client_t*        SV_GetCommandClient();

extern ServerData_t gServerData;
