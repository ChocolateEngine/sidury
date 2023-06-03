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

CONVAR( sv_server_name, "taco", CVARF_SERVER | CVARF_ARCHIVE );
CONVAR( sv_client_timeout, 30.f, CVARF_SERVER | CVARF_ARCHIVE );
CONVAR( sv_client_timeout_enable, 1, CVARF_SERVER | CVARF_ARCHIVE );

ServerData_t        gServerData;

static SV_Client_t* gpCommandClient = nullptr;

struct SV_ClientCommand_t
{
	SV_Client_t* apClient = nullptr;
	std::string  aCommands;
};

static std::vector< SV_ClientCommand_t > gClientCommandQueue;


int SV_Client_t::Read( char* spData, int sLen )
{
	return Net_Read( aSocket, spData, sLen, &aAddr );
}


int SV_Client_t::Write( const char* spData, int sLen )
{
	return Net_Write( aSocket, spData, sLen, &aAddr );
}


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
	// Create Server Physics and Entity System
	EntitySystem::CreateServer();

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

	EntitySystem::DestroyServer();

	gServerData.aActive = false;
	gServerData.aClients.clear();
}


void SV_Update( float frameTime )
{
	Game_SetClient( false );

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

		// Process incoming data from clients
		SV_ProcessClientMsg( client );
	}

	// Process console commands sent from clients
	// for ( auto& clientCmd : gClientCommandQueue )
	// {
	// 	SV_SetCommandClient( clientCmd.apClient );
	// 	Con_RunCommand( clientCmd.aCommands );
	// 	SV_SetCommandClient( nullptr );  // Clear it
	// }

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


void SV_SendMessageToClient2( SV_Client_t& srClient, capnp::MessageBuilder& srMessage, EMsgSrcServer sType )
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
	serverInfo.setName( sv_server_name.GetValue().data() );
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

	srClient.aState = ESV_ClientState_Disconnected;
}


void SV_SetCommandClient( SV_Client_t* spClient )
{
	gpCommandClient = spClient;
}


SV_Client_t* SV_GetCommandClient()
{
	return gpCommandClient;
}


Entity SV_GetCommandClientEntity()
{
	SV_Client_t* client = SV_GetCommandClient();

	if ( !client )
	{
		Log_Error( gLC_Server, "SV_GetCommandClientEntity(): No Command Client currently set!\n" );
		return CH_ENT_INVALID;
	}

	return client->aEntity;
}


SV_Client_t* SV_GetClientFromEntity( Entity sEntity )
{
	// oh boy
	for ( SV_Client_t& client : gServerData.aClients )
	{
		if ( client.aEntity == sEntity )
			return &client;
	}

	Log_Error( gLC_Server, "SV_GetClientFromEntity(): Failed to find entity attached to a client!\n" );
	return nullptr;
}


void SV_ProcessCommands( SV_Client_t& srClient )
{
}


void SV_HandleMsg_ClientInfo()
{
}


void SV_HandleMsg_UserCmd( SV_Client_t& srClient, NetMsgUserCmd::Reader& srReader )
{
	NetHelper_ReadVec3( srReader.getAngles(), srClient.aUserCmd.aAng );
	srClient.aUserCmd.aButtons  = srReader.getButtons();
	srClient.aUserCmd.aMoveType = static_cast< PlayerMoveType >( srReader.getMoveType() );
}


void SV_ProcessClientMsg( SV_Client_t& srClient )
{
	ChVector< char > data( 8192 );
	int              len = srClient.Read( data.data(), data.size() );

	// Timer here for each client to make sure they are connected
	// anything received from the client will reset their connection timer
	if ( len <= 0 )
	{
		// They haven't sent anything in a while, disconnect them
		if ( sv_client_timeout_enable && Game_GetCurTime() > srClient.aTimeout )
			SV_SendDisconnect( srClient );

		return;
	}

	// Reset the connection timer
	srClient.aTimeout = Game_GetCurTime() + sv_client_timeout;

	// Read the message sent from the client
	capnp::FlatArrayMessageReader reader( kj::ArrayPtr< const capnp::word >( (const capnp::word*)data.data(), data.size() ) );
	auto                          clientMsg = reader.getRoot< MsgSrcClient >();

	auto                          msgType   = clientMsg.getType();
	auto                          msgData   = clientMsg.getData();

	capnp::FlatArrayMessageReader dataReader( kj::ArrayPtr< const capnp::word >( (const capnp::word*)msgData.begin(), data.size() ) );

	switch ( msgType )
	{
		// Client is Disconnecting
		case EMsgSrcClient::DISCONNECT:
		{
			srClient.aState = ESV_ClientState_Disconnected;
			return;
		}

		case EMsgSrcClient::CLIENT_INFO:
		{
			SV_HandleMsg_ClientInfo();
			break;
		}

		case EMsgSrcClient::CON_VAR:
		{
			SV_SetCommandClient( &srClient );
			auto msgConVar = dataReader.getRoot< NetMsgConVar >();
			Game_ExecCommandsSafe( ECommandSource_Client, msgConVar.getCommand().cStr() );
			SV_SetCommandClient( nullptr );  // Clear it
			break;
		}

		case EMsgSrcClient::USER_CMD:
		{
			auto msgUserCmd = dataReader.getRoot< NetMsgUserCmd >();
			SV_HandleMsg_UserCmd( srClient, msgUserCmd );
			break;
		}

		default:
			// TODO: have a message type to string function
			Log_WarnF( gLC_Server, "Unknown Message Type from Client: %s\n", msgType );
			break;
	}
}


void SV_ConnectClientFinish( SV_Client_t& srClient )
{
	srClient.aState = ESV_ClientState_Connected;

	// Spawn the player in!
	players->Spawn( srClient.aEntity );
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

		// Connect to them with a new socket to communicate with them on
		int ret = Net_Connect( newSocket, clientAddr );
		if ( ret != 0 )
		{
			Net_CloseSocket( newSocket );
			continue;
		}
		
		// Get the port of this new socket
		ch_sockaddr newAddr;
		Net_GetSocketAddr( newSocket, newAddr );
		int          newPort = Net_GetSocketPort( newAddr );

		// Create a new client struct
		SV_Client_t& client  = gServerData.aClients.emplace_back();
		client.aName         = clientInfoRead.getName();
		client.aSocket       = newSocket;
		client.aAddr         = clientAddr;
		client.aState        = ESV_ClientState_Connecting;

		// Make an entity for them
		client.aEntity       = GetEntitySystem()->CreateEntity();

		players->Create( client.aEntity );

		// Send them the server info and new port
		capnp::MallocMessageBuilder message;
		SV_BuildServerInfo( message );

		NetMsgServerInfo::Builder serverInfo = message.getRoot< NetMsgServerInfo >();

		// hack, tell the client to use switch over to this port
		serverInfo.setNewPort( newPort );
		serverInfo.setPlayerEntityId( client.aEntity );

		auto array = capnp::messageToFlatArray( message );

		// send them this information on the listen socket, and with the port, the client and switch to that one for their connection
		int  write = Net_Write( listenSocket, array.asChars().begin(), array.size() * sizeof( capnp::word ), &clientAddr );

		gServerData.aClientsConnecting.push_back( &client );
	}
}

