#pragma once

#include "game_shared.h"
#include "entity.h"
#include "network/net_main.h"
#include "capnproto/sidury.capnp.h"

// 
// The Server, only runs if the engine is a dedicated server, or hosting on the client
// 

EXT_CVAR_FLAG( CVARF_SERVER );
EXT_CVAR_FLAG( CVARF_REPLICATED );

#define CVARF_SERVER_REPLICATED CVARF_SERVER | CVARF_REPLICATED
#define CVARF_DEF_SERVER_REPLICATED CVARF( SERVER ) | CVARF( REPLICATED )


using ClientHandle_t = unsigned int;

// Absolute Max Client Limit
constexpr int            CH_MAX_CLIENTS    = 32;

// Invalid ClientHandle_t
constexpr ClientHandle_t CH_INVALID_CLIENT = 0;

namespace capnp
{
	class MallocMessageBuilder;
}


struct UserCmd_t;


enum ESVClientState
{
	ESV_ClientState_Disconnected,
	ESV_ClientState_Connecting,
	ESV_ClientState_Connected,
};


struct SV_Client_t
{
	ch_sockaddr    aAddr;

	std::string    aName = "[unnamed]";
	ESVClientState aState;

	double         aTimeout;

	Entity         aEntity = CH_ENT_INVALID;

	UserCmd_t      aUserCmd;

	int            Read( char* spData, int sLen );

	int            Write( const char* spData, int sLen );
	int            Write( const ChVector< char >& srData );
	int            WritePacked( capnp::MessageBuilder& srBuilder );

	bool           operator==( const SV_Client_t& srOther )
	{
		if ( this == &srOther )
			return true;

		if ( memcmp( this, &srOther, sizeof( SV_Client_t ) ) == 0 )
			return true;

		if ( memcmp( aAddr.sa_data, srOther.aAddr.sa_data, sizeof( aAddr.sa_data ) ) != 0 )
			return false;

		if ( aAddr.sa_family != srOther.aAddr.sa_family )
			return false;

		if ( aName != srOther.aName )
			return false;

		if ( aTimeout != aTimeout )
			return false;

		if ( aEntity != aEntity )
			return false;

		if ( memcmp( &aUserCmd, &srOther.aUserCmd, sizeof( aUserCmd ) ) != 0 )
			return false;

		return true;
	}
};


struct ServerData_t
{
	bool                                               aActive;

	std::vector< SV_Client_t >                         aClients;

	// Fixed handles to a client, so the indexes can easily change
	// This also allows you to change max clients live in game
	std::unordered_map< ClientHandle_t, SV_Client_t* > aClientIDs;
	std::unordered_map< SV_Client_t*, ClientHandle_t > aClientToIDs;

	// Clients that are still connecting
	ChVector< SV_Client_t* >                           aClientsConnecting;

	// Clients that want a full update
	ChVector< SV_Client_t* >                           aClientsFullUpdate;
};

// --------------------------------------------------------------------

bool                SV_Init();
void                SV_Shutdown();
void                SV_Update( float frameTime );
void                SV_GameUpdate( float frameTime );

bool                SV_StartServer();
void                SV_StopServer();

// --------------------------------------------------------------------
// Networking

int                 SV_BroadcastMsgsToSpecificClients( std::vector< capnp::MallocMessageBuilder >& srMessages, const ChVector< SV_Client_t* >& srClients );
int                 SV_BroadcastMsgs( std::vector< capnp::MallocMessageBuilder >& srMessages );
int                 SV_BroadcastMsg( capnp::MessageBuilder& srMessage );

bool                SV_SendMessageToClient( SV_Client_t& srClient, capnp::MessageBuilder& srMessage );
void                SV_SendDisconnect( SV_Client_t& srClient );

bool                SV_BuildServerMsg( capnp::MessageBuilder& srMessage, EMsgSrcServer sSrcType, bool sFullUpdate = false );

void                SV_ProcessSocketMsgs();
void                SV_ProcessClientMsg( SV_Client_t& srClient, capnp::MessageReader& srReader );

SV_Client_t*        SV_AllocateClient();
void                SV_FreeClient( SV_Client_t& srClient );

void                SV_ConnectClient( ch_sockaddr& srAddr, ChVector< char >& srData );
void                SV_ConnectClientFinish( SV_Client_t& srClient );

// --------------------------------------------------------------------
// Helper Functions

// size_t              SV_GetClientCount();
SV_Client_t*        SV_GetClient( ClientHandle_t sClient );

void                SV_SetCommandClient( SV_Client_t* spClient );
SV_Client_t*        SV_GetCommandClient();

Entity              SV_GetCommandClientEntity();
SV_Client_t*        SV_GetClientFromEntity( Entity sEntity );

Entity              SV_GetPlayerEntFromIndex( size_t sIndex );
Entity              SV_GetPlayerEnt( ClientHandle_t sClient );

void                SV_PrintStatus();

// --------------------------------------------------------------------

extern ServerData_t gServerData;

