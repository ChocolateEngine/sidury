#include "sv_main.h"
#include "game_shared.h"
#include "main.h"
#include "mapmanager.h"
#include "entity.h"
#include "player.h"

#include <capnp/message.h>
#include <capnp/serialize-packed.h>

//
// The Server, only runs if the engine is a dedicated server, or hosting on the client
//

LOG_REGISTER_CHANNEL2( Server, LogColor::Green );

NEW_CVAR_FLAG( CVARF_SERVER );

CONVAR( sv_servername, "taco", CVARF_SERVER | CVARF_ARCHIVE );

ServerData_t                   gServerData;

static SV_Client_t*            gpCommandClient = nullptr;

struct SV_ClientCommand_t
{
	SV_Client_t* apClient = nullptr;
	std::string  aCommands;
};

static std::vector< SV_ClientCommand_t > gClientCommandQueue;


// capnp::FlatArrayMessageReader& SV_GetReader( Socket_t sSocket, ChVector< char >& srData, ch_sockaddr& srAddr )
// {
// 	int len = Net_Read( sSocket, srData.data(), srData.size(), &srAddr );
// 
// 	if ( len < 0 )
// 		return;
// 
// 	capnp::FlatArrayMessageReader reader( kj::ArrayPtr< const capnp::word >( (const capnp::word*)srData.data(), srData.size() ) );
// 	return reader;
// }


bool SV_Init()
{
	// temp
	if ( !Net_OpenServer() )
	{
		Log_Error( gLC_Server, "Failed to Open Test Server\n" );
		return false;
	}

	gServerData.aActive = true;

	return true;
}


void SV_Shutdown()
{
	for ( auto& client : gServerData.aClients )
	{
		SV_SendDisconnect( client );
	}

	gServerData.aActive = false;
	gServerData.aClients.clear();
}


void SV_Update( float frameTime )
{
	SV_CheckForNewClients();

	// for ( auto& client : gServerData.aClients )
	for ( size_t i = 0; i < gServerData.aClients.size(); i++ )
	{
		auto& client = gServerData.aClients[ i ];

		// Continue connecting clients if any are joining
		if ( client.aState == ESV_ClientState_Connecting )
		{
			SV_ConnectClientFinish( client );
			continue;
		}

		if ( client.aState != ESV_ClientState_Connected )
		{
			// Remove this client from the list
			vec_remove_index( gServerData.aClients, i );
			i--;
			continue;
		}

		// Process incoming data from clients and UserCmd's
		SV_ProcessClient( client );
	}

	// Process console commands sent from clients
	for ( auto& clientCmd : gClientCommandQueue )
	{
		SV_SetCommandClient( clientCmd.apClient );
		Con_RunCommand( clientCmd.aCommands );
		SV_SetCommandClient( nullptr );  // Clear it
	}

	gClientCommandQueue.clear();
	
	// Main game loop
	SV_GameUpdate( frameTime );

	// TEMP - REMOVE ONCE YOU HAVE FRAME UPDATE DATA
	return;

	// Send updated data to clients
	capnp::MallocMessageBuilder message;
	SV_BuildUpdatedData( message );
	auto array = capnp::messageToFlatArray( message );

	for ( auto& client : gServerData.aClients )
	{
		if ( client.aState != ESV_ClientState_Connected )
			continue;

		int write = Net_Write( client.aSocket, array.asChars().begin(), array.size() * sizeof( capnp::word ), &client.aAddr );

		// If we failed to write, disconnect them?
		if ( write != 0 )
		{
			Log_ErrorF( gLC_Server, "Failed to write network data to client, marking client as disconnected: %s\n", Net_ErrorString() );
			client.aState = ESV_ClientState_Disconnected;
		}
	}
}


void SV_GameUpdate( float frameTime )
{
}


void SV_BuildUpdatedData( capnp::MessageBuilder& srMessage )
{
}


void SV_SendMessageToClient( SV_Client_t& srClient, capnp::MessageBuilder& srMessage )
{
	auto array = capnp::messageToFlatArray( srMessage );
	int  write = Net_Write( srClient.aSocket, array.asChars().begin(), array.size() * sizeof( capnp::word ), &srClient.aAddr );
}


// hi im a server here's a taco - agrimar
void SV_BuildServerInfo( capnp::MessageBuilder& srMessage )
{
	NetMsgServerInfo::Builder serverInfo = srMessage.initRoot< NetMsgServerInfo >();

	serverInfo.setProtocol( CH_SERVER_PROTOCOL_VER );
	serverInfo.setMapName( MapManager_GetMapName().data() );
	serverInfo.setMapHash( "todo" );
	serverInfo.setName( sv_servername.GetValue().data() );
	serverInfo.setPlayerCount( gServerData.aClients.size() );

	// hack
	serverInfo.setNewPort( -1 );
}


void SV_SendServerInfo( SV_Client_t& srClient )
{
	capnp::MallocMessageBuilder message;
	SV_BuildServerInfo( message );
	SV_SendMessageToClient( srClient, message );
}


void SV_SendDisconnect( SV_Client_t& srClient )
{
	capnp::MallocMessageBuilder message;
	NetMsgDisconnect::Builder     disconnectMsg = message.initRoot< NetMsgDisconnect >();

	disconnectMsg.setReason( "Saving chunks." );

	SV_SendMessageToClient( srClient, message );
}


void SV_SetCommandClient( SV_Client_t* spClient )
{
	gpCommandClient = spClient;
}


SV_Client_t* SV_GetCommandClient()
{
	return gpCommandClient;
}


void SV_ProcessCommands( SV_Client_t& srClient )
{
}


void SV_ProcessClient( SV_Client_t& srClient )
{
}


void SV_ConnectClientFinish( SV_Client_t& srClient )
{
}


void SV_CheckForNewClients()
{
	// Check for all incoming connections until none are left
	while ( true )
	{
		Socket_t listenSocket = Net_CheckNewConnections();
		if ( listenSocket == CH_INVALID_SOCKET )
			break;

		ch_sockaddr      clientAddr;
		ChVector< char > data( 8192 );
		int              len = Net_Read( listenSocket, data.data(), data.size(), &clientAddr );
		
		if ( len < 0 )
			continue;

		// Get Client Info
		capnp::FlatArrayMessageReader reader( kj::ArrayPtr< const capnp::word >( (const capnp::word*)data.data(), data.size() ) );
		NetMsgClientInfo::Reader      clientInfoRead = reader.getRoot< NetMsgClientInfo >();

		// TODO: check to see if they are already connected?
		// TODO: also check to see if we are at the max player count

		// Open a new socket for them
		Socket_t                      newSocket      = Net_OpenSocket( "0" );

		// Connect to them
		int ret = Net_Connect( newSocket, clientAddr );
		if ( ret != 0 )
		{
			Net_CloseSocket( newSocket );
			continue;
		}
		
		ch_sockaddr newAddr;
		Net_GetSocketAddr( newSocket, newAddr );

		int newPort = Net_GetSocketPort( newAddr );

		// Create a new client struct
		SV_Client_t& client = gServerData.aClients.emplace_back();
		client.aName        = clientInfoRead.getName();
		client.aSocket      = newSocket;
		client.aAddr        = clientAddr;
		client.aState       = ESV_ClientState_Connecting;

		// Send them the server info and new port
		capnp::MallocMessageBuilder message;
		SV_BuildServerInfo( message );

		NetMsgServerInfo::Builder serverInfo = message.getRoot< NetMsgServerInfo >();

		// hack
		serverInfo.setNewPort( newPort );

		auto array = capnp::messageToFlatArray( message );

		// send them this information on the listen socket, and with the port, the client and switch to that one for their connection
		int  write = Net_Write( listenSocket, array.asChars().begin(), array.size() * sizeof( capnp::word ), &clientAddr );

		gServerData.aClientsConnecting.push_back( &client );
	}
}

