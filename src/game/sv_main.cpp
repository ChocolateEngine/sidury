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

static const char* gTestServerIP   = Args_Register( "127.0.0.1", "Test Server IPv4", "-ip" );
static const char* gTestServerPort = Args_Register( "27016", "Test Server Port", "-port" );

NEW_CVAR_FLAG( CVARF_SERVER );

CONVAR( sv_server_name, "taco", CVARF_SERVER | CVARF_ARCHIVE );
CONVAR( sv_client_timeout, 30.f, CVARF_SERVER | CVARF_ARCHIVE );
CONVAR( sv_client_timeout_enable, 1, CVARF_SERVER | CVARF_ARCHIVE );

ServerData_t        gServerData;

static Socket_t     gServerSocket   = CH_INVALID_SOCKET;

static SV_Client_t* gpCommandClient = nullptr;

struct SV_ClientCommand_t
{
	SV_Client_t* apClient = nullptr;
	std::string  aCommands;
};

static std::vector< SV_ClientCommand_t > gClientCommandQueue;


CONCMD( pause )
{
	Game_SetPaused( !Game_IsPaused() );
}


int SV_Client_t::Read( char* spData, int sLen )
{
	return Net_Read( gServerSocket, spData, sLen, &aAddr );
}


int SV_Client_t::Write( const char* spData, int sLen )
{
	return Net_Write( gServerSocket, spData, sLen, &aAddr );
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
	return true;
}


void SV_Shutdown()
{
	SV_StopServer();
}


void SV_Update( float frameTime )
{
	Game_SetClient( false );

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
	}

	SV_ProcessSocketMsgs();

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
	// return;

	// Send updated data to clients
	capnp::MallocMessageBuilder message;
	SV_BuildUpdatedData( message );
	auto array = capnp::messageToFlatArray( message );

	for ( auto& client : gServerData.aClients )
	{
		if ( client.aState != ESV_ClientState_Connected )
			continue;

		// int write = Net_Write( client.aSocket, array.asChars().begin(), array.size() * sizeof( capnp::word ), &client.aAddr );
		int write = client.Write( array.asChars().begin(), array.size() * sizeof( capnp::word ) );

		// If we failed to write, disconnect them?
		if ( write == 0 )
		{
			Log_ErrorF( gLC_Server, "Failed to write network data to client, marking client as disconnected: %s\n", Net_ErrorString() );
			client.aState = ESV_ClientState_Disconnected;
		}
	}
}


void SV_GameUpdate( float frameTime )
{
	MapManager_Update();

	GetPlayers()->Update( frameTime );

	Phys_Simulate( GetPhysEnv(), frameTime );

	// TEST_EntUpdate();

	// Update player positions after physics simulation
	// NOTE: This probably needs to be done for everything with physics
	for ( auto& player : GetPlayers()->aPlayerList )
	{
		GetPlayers()->apMove->UpdatePosition( player );
	}
}


bool SV_StartServer()
{
	SV_StopServer();

	// Create Server Physics and Entity System
	if ( !EntitySystem::CreateServer() )
	{
		Log_ErrorF( "Failed to Create Server Entity System\n" );
		return false;
	}

	PlayerManager::CreateServer();

	Phys_CreateEnv( false );

	gServerSocket = Net_OpenSocket( gTestServerPort );

	if ( gServerSocket == CH_INVALID_SOCKET )
	{
		Log_Error( gLC_Server, "Failed to Open Test Server\n" );
		return false;
	}

	GetPlayers()->Init();

	gServerData.aActive = true;
	return true;
}


void SV_StopServer()
{
	for ( auto& client : gServerData.aClients )
	{
		SV_SendDisconnect( client );
	}

	EntitySystem::DestroyServer();
	PlayerManager::DestroyServer();

	Phys_DestroyEnv( false );

	gServerData.aActive = false;
	gServerData.aClients.clear();

	Net_CloseSocket( gServerSocket );
	gServerSocket = CH_INVALID_SOCKET;
}


void SV_BuildEntityList( capnp::MessageBuilder& srBuilder )
{
	GetEntitySystem()->WriteEntityUpdates( srBuilder );
}


void SV_BuildUpdatedData( capnp::MessageBuilder& srBuilder )
{
	// if ( GetEntitySystem()->aEntityCount == 0 )
	// 	return;

	// TODO: do more than just entity list, allow a list of messages from the server in one go
	auto root = srBuilder.initRoot< MsgSrcServer >();
	root.setType( EMsgSrcServer::ENTITY_LIST );

	capnp::MallocMessageBuilder entListBuilder;
	SV_BuildEntityList( entListBuilder );

	auto array = capnp::messageToFlatArray( entListBuilder );
	auto data  = root.initData( array.size() * sizeof( capnp::word ) );

	// This is probably awful and highly inefficent
	// std::copy( array.begin(), array.end(), (capnp::word*)data.begin() );
	memcpy( data.begin(), array.begin(), array.size() * sizeof( capnp::word ) );

	//Log_DevF( gLC_Server, 2, "Sending ENTITY_LIST to Clients\n" );
}


void SV_SendMessageToClient( SV_Client_t& srClient, capnp::MessageBuilder& srMessage )
{
	auto array = capnp::messageToFlatArray( srMessage );
	int  write = Net_Write( gServerSocket, array.asChars().begin(), array.size() * sizeof( capnp::word ), &srClient.aAddr );
}


void SV_SendMessageToClient2( SV_Client_t& srClient, capnp::MessageBuilder& srMessage, EMsgSrcServer sType )
{
	auto array = capnp::messageToFlatArray( srMessage );
	int  write = Net_Write( gServerSocket, array.asChars().begin(), array.size() * sizeof( capnp::word ), &srClient.aAddr );
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

	Log_DevF( gLC_Server, 1, "Building Server Info\n" );
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

	Log_MsgF( gLC_Server, "Disconnecting Client: \"%s\"\n", srClient.aName.c_str() );
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

	Log_ErrorF( gLC_Server, "SV_GetClientFromEntity(): Failed to find entity attached to a client! (Entity %zd)\n", sEntity );
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
	Log_DevF( gLC_Server, 2, "Handling Message USER_CMD from Client \"%s\"\n", srClient.aName.c_str() );

	NetHelper_ReadVec3( srReader.getAngles(), srClient.aUserCmd.aAng );
	srClient.aUserCmd.aButtons    = srReader.getButtons();
	srClient.aUserCmd.aFlashlight = srReader.getFlashlight();
	srClient.aUserCmd.aMoveType   = static_cast< PlayerMoveType >( srReader.getMoveType() );
}


void SV_ProcessClientMsg( SV_Client_t& srClient, capnp::MessageReader& srReader )
{
	// TODO: move this connection timer elsewhere
#if 0
	// Timer here for each client to make sure they are connected
	// anything received from the client will reset their connection timer
	if ( len <= 0 )
	{
		// They haven't sent anything in a while, disconnect them
		if ( sv_client_timeout_enable && Game_GetCurTime() > srClient.aTimeout )
			SV_SendDisconnect( srClient );

		srClient.aTimeout -= gFrameTime;

		return;
	}
#endif

	// Reset the connection timer
	srClient.aTimeout = Game_GetCurTime() + sv_client_timeout;

	// Read the message sent from the client
	auto                          clientMsg = srReader.getRoot< MsgSrcClient >();

	EMsgSrcClient                 msgType   = clientMsg.getType();
	auto                          msgData   = clientMsg.getData();

	capnp::FlatArrayMessageReader dataReader( kj::ArrayPtr< const capnp::word >( (const capnp::word*)msgData.begin(), msgData.size() ) );

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


SV_Client_t* SV_GetClientFromAddr( ch_sockaddr& srAddr )
{
	for ( auto& client : gServerData.aClients )
	{
		// if ( client.aAddr.sa_data == clientAddr.sa_data && client.aAddr.sa_family == clientAddr.sa_family )
		// if ( memcmp( client.aAddr.sa_data, clientAddr.sa_data ) == 0 && client.aAddr.sa_family == clientAddr.sa_family )
		if ( memcmp( client.aAddr.sa_data, srAddr.sa_data, sizeof( client.aAddr.sa_data ) ) == 0 )
		{
			return &client;
		}
	}

	return nullptr;
}


void SV_ProcessSocketMsgs()
{
	while ( true )
	{
		ChVector< char > data( 8192 );
		ch_sockaddr      clientAddr;
		int              len = Net_Read( gServerSocket, data.data(), data.size(), &clientAddr );

		if ( len <= 0 )
			return;

		SV_Client_t* client = SV_GetClientFromAddr( clientAddr );

		if ( !client )
		{
			SV_ConnectClient( clientAddr, data );
			continue;
		}

		// Reset the connection timer
		client->aTimeout = Game_GetCurTime() + sv_client_timeout;

		// Read the message sent from the client
		capnp::FlatArrayMessageReader reader( kj::ArrayPtr< const capnp::word >( (const capnp::word*)data.data(), data.size() ) );
		SV_ProcessClientMsg( *client, reader );
	}
}


void SV_ConnectClientFinish( SV_Client_t& srClient )
{
	srClient.aState = ESV_ClientState_Connected;

	vec_remove( gServerData.aClientsConnecting, &srClient );

	// Spawn the player in!
	GetPlayers()->Spawn( srClient.aEntity );

	// Reset the connection timer
	srClient.aTimeout = Game_GetCurTime() + sv_client_timeout;
}


void SV_ConnectClient( ch_sockaddr& srAddr, ChVector< char >& srData )
{
	// Get Client Info
	capnp::FlatArrayMessageReader reader( kj::ArrayPtr< const capnp::word >( (const capnp::word*)srData.data(), srData.size() ) );
	NetMsgClientInfo::Reader      clientInfoRead = reader.getRoot< NetMsgClientInfo >();

	SV_Client_t&                  client         = gServerData.aClients.emplace_back();
	client.aName                                 = clientInfoRead.getName();
	client.aAddr                                 = srAddr;
	client.aState                                = ESV_ClientState_Connecting;

	// Make an entity for them
	client.aEntity      = GetEntitySystem()->CreateEntity();

	GetPlayers()->Create( client.aEntity );

	// Send them the server info and new port
	capnp::MallocMessageBuilder message;
	SV_BuildServerInfo( message );

	NetMsgServerInfo::Builder serverInfo = message.getRoot< NetMsgServerInfo >();

	serverInfo.setPlayerEntityId( client.aEntity );

	auto array = capnp::messageToFlatArray( message );

	// send them this information on the listen socket, and with the port, the client and switch to that one for their connection
	int  write = Net_Write( gServerSocket, array.asChars().begin(), array.size() * sizeof( capnp::word ), &srAddr );

	gServerData.aClientsConnecting.push_back( &client );
}

