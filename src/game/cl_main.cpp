#include "main.h"
#include "cl_main.h"
#include "game_shared.h"
#include "mapmanager.h"
#include "network/net_main.h"
#include "capnproto/sidury.capnp.h"

#include <capnp/message.h>
#include <capnp/serialize-packed.h>

//
// The Client, always running unless a dedicated server
// 

LOG_REGISTER_CHANNEL2( Client, LogColor::White );

static Socket_t                   gClientSocket = CH_INVALID_SOCKET;
static ch_sockaddr                gClientAddr;

static EClientState               gClientState = EClientState_Idle;
static CL_ServerData_t            gClientServerData;

// How much time we have until we give up connecting if the server doesn't respond anymore
static double                     gClientConnectTimeout = 0.f;

// Console Commands to send to the server to process, like noclip
static std::vector< std::string > gCommandsToSend;

// NEW_CVAR_FLAG( CVARF_CLIENT );

CONVAR( cl_timeout_duration, 30.f );

CONVAR_CMD_EX( cl_username, "greg", CVARF_ARCHIVE, "Your Username" )
{
	if ( cl_username.GetValue().size() <= CH_MAX_USERNAME_LEN )
		return;

	Log_WarnF( gLC_Client, "Username is too long (%zd chars), max is %d chars\n", cl_username.GetValue().size(), CH_MAX_USERNAME_LEN );
	cl_username.SetValue( prevString );
}


CONCMD( connect )
{
	if ( args.empty() )
	{
		Log_Msg( gLC_Client, "Type in an IP address after the word connect\n" );
		return;
	}

	CL_Connect( args[ 0 ].data() );
}


bool CL_Init()
{
	// connect to the loopback server
	CL_Connect( "127.0.0.1" );

	return true;
}


void CL_Shutdown()
{
	CL_Disconnect();
}


bool CL_RecvServerInfo()
{
	ChVector< char > data( 8192 );
	int              len = Net_Read( gClientSocket, data.data(), data.size(), &gClientAddr );

	if ( len <= 0 )
	{
		// NOTE: this might get hit, we need some sort of retry thing
		Log_Warn( gLC_Client, "No Server Info\n" );

		if ( gClientConnectTimeout < Game_GetCurTime() )
			return false;

		// keep waiting i guess?
		return true;
	}

	capnp::FlatArrayMessageReader reader( kj::ArrayPtr< const capnp::word >( (const capnp::word*)data.data(), data.size() ) );
	NetMsgServerInfo::Reader      serverInfoMsg = reader.getRoot< NetMsgServerInfo >();

	// hack, should not be part of this server info message, should be a message prior to this
	// will set up later
	if ( serverInfoMsg.getNewPort() != -1 )
	{
		Net_SetSocketPort( gClientAddr, serverInfoMsg.getNewPort() );
	}

	gClientServerData.aName                     = serverInfoMsg.getName();
	gClientServerData.aMapName                  = serverInfoMsg.getMapName();
	gClientState                                = EClientState_Connecting;

	return true;
}


void CL_Update( float frameTime )
{
	switch ( gClientState )
	{
		default:
		case EClientState_Idle:
			break;

		case EClientState_RecvServerInfo:
		{
			// Recieve Server Info first
			if ( !CL_RecvServerInfo() )
				CL_Disconnect();

			break;
		}

		case EClientState_Connecting:
		{
			// Try to load the map
			if ( !Game_IsHosting() )
			{
				// Do we have the map?
				if ( !MapManager_FindMap( gClientServerData.aMapName ) )
				{
					// Maybe one day we can download the map from the server
					CL_Disconnect( "Missing Map" );
					break;
				}

				// Load Map (MAKE THIS ASYNC)
				if ( !MapManager_LoadMap( gClientServerData.aMapName ) )
				{
					CL_Disconnect( "Failed to Load Map" );
					break;
				}
			}

			break;
		}

		case EClientState_Connected:
		{
			// Process Stuff from server
			CL_GetServerMessages();

			CL_GameUpdate( frameTime );

			// Send Console Commands
			if ( gCommandsToSend.size() )
			{
				std::string command;

				// Join it all into one string
				for ( auto& cmd : gCommandsToSend )
					command += cmd + ";";
			}

			// Send UserCmd
			CL_CreateUserCmd();
			break;
		}
	}
}


void CL_Disconnect( const char* spReason )
{
	if ( gClientSocket != CH_INVALID_SOCKET )
	{
		if ( spReason )
		{
			// Tell the server why we are disconnecting
			::capnp::MallocMessageBuilder message;
			NetMsgClientInfo::Builder     clientInfoBuild = message.initRoot< NetMsgClientInfo >();

		}

		Net_CloseSocket( gClientSocket );
		gClientSocket = CH_INVALID_SOCKET;
	}

	memset( &gClientAddr, 0, sizeof( gClientAddr ) );

	gClientState          = EClientState_Idle;
	gClientConnectTimeout = 0.f;
}


// TODO: should only be platform specific, needs to have sockaddr abstracted
extern void Net_NetadrToSockaddr( const NetAddr_t* spNetAddr, struct sockaddr* spSockAddr );

void CL_Connect( const char* spAddress )
{
	// Make sure we are not connected to a server already
	CL_Disconnect();

	::capnp::MallocMessageBuilder message;
	NetMsgClientInfo::Builder     clientInfoBuild = message.initRoot< NetMsgClientInfo >();

	clientInfoBuild.setName( cl_username.GetValue().data() );

	gClientSocket     = Net_OpenSocket( "0" );
	NetAddr_t netAddr = Net_GetNetAddrFromString( spAddress );

	Net_NetadrToSockaddr( &netAddr, (struct sockaddr*)&gClientAddr );

	int  what  = Net_Connect( gClientSocket, gClientAddr );

	// kj::HandleOutputStream out( (HANDLE)gClientSocket );
	// capnp::writeMessage( out, message );

	auto array = capnp::messageToFlatArray( message );

	int  write = Net_Write( gClientSocket, array.asChars().begin(), array.size() * sizeof( capnp::word ), &gClientAddr );
	// int  write = Net_Write( gClientSocket, cl_username.GetValue().data(), cl_username.GetValue().size() * sizeof( char ), &sockAddr );

	// Continue connecting in CL_Update()
	gClientState          = EClientState_RecvServerInfo;
	gClientConnectTimeout = Game_GetCurTime() + cl_timeout_duration;
}


void CL_GameUpdate( float frameTime )
{
}


void CL_CreateUserCmd()
{
	UserCmd_t userCmd;
	userCmd.aAng;
}


void CL_GetServerMessages()
{
}
