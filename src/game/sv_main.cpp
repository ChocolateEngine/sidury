#include "sv_main.h"
#include "game_shared.h"
#include "main.h"
#include "mapmanager.h"
#include "entity.h"
#include "player.h"

#include "testing.h"
#include "igui.h"

#include <capnp/message.h>
#include <capnp/serialize-packed.h>

//
// The Server, only runs if the engine is a dedicated server, or hosting on the client
//

LOG_REGISTER_CHANNEL2( Server, LogColor::Green );

static const char* gServerPort = Args_Register( "41628", "Test Server Port", "-port" );

NEW_CVAR_FLAG( CVARF_SERVER );

CONVAR( sv_server_name, "taco", CVARF_SERVER | CVARF_ARCHIVE );
CONVAR( sv_client_timeout, 30.f, CVARF_SERVER | CVARF_ARCHIVE );
CONVAR( sv_client_timeout_enable, 1, CVARF_SERVER | CVARF_ARCHIVE );
CONVAR( sv_pause, 0, CVARF_SERVER, "Pauses the Server Code" );

ServerData_t        gServerData;

static Socket_t     gServerSocket   = CH_INVALID_SOCKET;

static SV_Client_t* gpCommandClient = nullptr;


CONCMD( pause )
{
	if ( !Game_IsHosting() )
		return;

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


int SV_Client_t::Write( const ChVector< char >& srData )
{
	return Net_Write( gServerSocket, srData.begin(), srData.size_bytes(), &aAddr );
}


int SV_Client_t::WritePacked( capnp::MessageBuilder& srBuilder )
{
	return Net_WritePacked( gServerSocket, aAddr, srBuilder );
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
	if ( sv_pause )
		return;

	Game_SetClient( false );

	// for ( auto& client : gServerData.aClients )
	for ( size_t i = 0; i < gServerData.aClients.size(); i++ )
	{
		SV_Client_t& client = gServerData.aClients[ i ];

		// Continue connecting clients if any are joining
		if ( client.aState == ESV_ClientState_Connecting )
		{
			SV_ConnectClientFinish( client );
			continue;
		}

		if ( client.aState != ESV_ClientState_Connected )
		{
			// Remove their player entity
			GetEntitySystem()->DeleteEntity( client.aEntity );

			// Remove this client from the list
			vec_remove_index( gServerData.aClients, i );
			i--;
			continue;
		}
	}

	SV_ProcessSocketMsgs();
	
	// Main game loop
	SV_GameUpdate( frameTime );

	// TODO: Sync Server Convars with clients

	// Send updated data to clients
	std::vector< capnp::MallocMessageBuilder > messages( 2 );

	SV_BuildServerMsg( messages[ 0 ], EMsgSrcServer::ENTITY_LIST, false );
	SV_BuildServerMsg( messages[ 1 ], EMsgSrcServer::COMPONENT_LIST, false );

	int writeSize = SV_BroadcastMsgs( messages );

	// Check to see if anyone needs a full update
	if ( gServerData.aClientsFullUpdate.size() )
	{
		// Build a Full Update and send it to all of them
		std::vector< capnp::MallocMessageBuilder > messages( 2 );

		SV_BuildServerMsg( messages[ 0 ], EMsgSrcServer::ENTITY_LIST, true );
		SV_BuildServerMsg( messages[ 1 ], EMsgSrcServer::COMPONENT_LIST, true );

		int fullUpdateSize = SV_BroadcastMsgsToSpecificClients( messages, gServerData.aClientsFullUpdate );
		Log_DevF( gLC_Server, 1, "Writing Full Update to Clients: %d bytes\n", fullUpdateSize );

		gServerData.aClientsFullUpdate.clear();
	}

	// Update Entity and Component States after everything is processed
	GetEntitySystem()->UpdateStates();

	if ( Game_IsClient() )
	{
		gui->DebugMessage( "Data Written to Each Client: %d bytes", writeSize );
	}
}


void SV_GameUpdate( float frameTime )
{
	GetEntitySystem()->UpdateSystems();

	MapManager_Update();

	GetPlayers()->Update( frameTime );

	Phys_Simulate( GetPhysEnv(), frameTime );

	TEST_SV_UpdateProtos( frameTime );

	// Update player positions after physics simulation
	// NOTE: This probably needs to be done for everything with physics
	for ( auto& player : GetPlayers()->aEntities )
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

	Phys_CreateEnv( false );

	gServerSocket = Net_OpenSocket( gServerPort );

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
	bool wasClient = Game_ProcessingClient();
	Game_SetClient( false );

	for ( auto& client : gServerData.aClients )
	{
		SV_SendDisconnect( client );
	}

	MapManager_CloseMap();

	EntitySystem::DestroyServer();

	Phys_DestroyEnv( false );

	gServerData.aActive = false;
	gServerData.aClients.clear();

	Net_CloseSocket( gServerSocket );
	gServerSocket = CH_INVALID_SOCKET;

	Game_SetClient( wasClient );
}


// --------------------------------------------------------------------
// Networking


int SV_BroadcastMsgsToSpecificClients( std::vector< capnp::MallocMessageBuilder >& srMessages, const ChVector< SV_Client_t* >& srClients )
{
	std::vector< NetOutputStream > outStreams( srMessages.size() );
	int                            writeSize = 0;

	for ( size_t i = 0; i < srMessages.size(); i++ )
	{
		capnp::writePackedMessage( outStreams[ i ], srMessages[ i ] );
		writeSize += outStreams[ i ].aBuffer.size_bytes();
	}

	for ( auto client : srClients )
	{
		if ( !client )
			continue;

		if ( client->aState != ESV_ClientState_Connected )
			continue;

		for ( size_t arrayIndex = 0; arrayIndex < outStreams.size(); arrayIndex++ )
		{
			int write = client->Write( outStreams[ arrayIndex ].aBuffer );

			// If we failed to write, disconnect them?
			if ( write == 0 )
			{
				Log_ErrorF( gLC_Server, "Failed to write network data to client, marking client as disconnected: %s\n", Net_ErrorString() );
				client->aState = ESV_ClientState_Disconnected;
				break;
			}
		}
	}

	return writeSize;
}


int SV_BroadcastMsgs( std::vector< capnp::MallocMessageBuilder >& srMessages )
{
	std::vector< NetOutputStream > outStreams( srMessages.size() );
	int                            writeSize = 0;

	for ( size_t i = 0; i < srMessages.size(); i++ )
	{
		capnp::writePackedMessage( outStreams[ i ], srMessages[ i ] );
		writeSize += outStreams[ i ].aBuffer.size_bytes();
	}

	for ( auto& client : gServerData.aClients )
	{
		if ( client.aState != ESV_ClientState_Connected )
			continue;

		for ( size_t arrayIndex = 0; arrayIndex < outStreams.size(); arrayIndex++ )
		{
			int write = client.Write( outStreams[ arrayIndex ].aBuffer );

			// If we failed to write, disconnect them?
			if ( write == 0 )
			{
				Log_ErrorF( gLC_Server, "Failed to write network data to client, marking client as disconnected: %s\n", Net_ErrorString() );
				client.aState = ESV_ClientState_Disconnected;
				break;
			}
		}
	}

	return writeSize;
}


int SV_BroadcastMsg( capnp::MessageBuilder& srMessage )
{
	NetOutputStream outputStream;
	capnp::writePackedMessage( outputStream, srMessage );

	for ( auto& client : gServerData.aClients )
	{
		if ( client.aState != ESV_ClientState_Connected )
			continue;

		int write = client.Write( outputStream.aBuffer );

		// If we failed to write, disconnect them?
		if ( write == 0 )
		{
			Log_ErrorF( gLC_Server, "Failed to write network data to client, marking client as disconnected: %s\n", Net_ErrorString() );
			client.aState = ESV_ClientState_Disconnected;
		}
	}

	return outputStream.aBuffer.size_bytes();
}


void SV_BuildServerMsg( capnp::MessageBuilder& srBuilder, EMsgSrcServer sSrcType, bool sFullUpdate )
{
	auto root = srBuilder.initRoot< MsgSrcServer >();

	capnp::MallocMessageBuilder messageBuilder;

	switch ( sSrcType )
	{
		case EMsgSrcServer::ENTITY_LIST:
		{
			root.setType( EMsgSrcServer::ENTITY_LIST );
			GetEntitySystem()->WriteEntityUpdates( messageBuilder );
			//Log_DevF( gLC_Server, 2, "Sending ENTITY_LIST to Clients\n" );
			break;
		}
		case EMsgSrcServer::COMPONENT_LIST:
		{
			root.setType( EMsgSrcServer::COMPONENT_LIST );
			GetEntitySystem()->WriteComponentUpdates( messageBuilder, sFullUpdate );
			//Log_DevF( gLC_Server, 2, "Sending COMPONENT_LIST to Clients\n" );
			break;
		}
	}
	
	NetOutputStream outputStream;
	capnp::writePackedMessage( outputStream, messageBuilder );

	auto data = root.initData( outputStream.aBuffer.size_bytes() );
	memcpy( data.begin(), outputStream.aBuffer.begin(), outputStream.aBuffer.size_bytes() );
}


#if 0
void SV_BuildUpdatedData( bool sFullUpdate )
{
	// Send updated data to clients
	std::vector< capnp::MallocMessageBuilder > messages( 2 );
	std::vector< kj::Array< capnp::word > >    arrays( 2 );

	SV_BuildServerMsg( messages[ 0 ], EMsgSrcServer::ENTITY_LIST, sFullUpdate );
	SV_BuildServerMsg( messages[ 1 ], EMsgSrcServer::COMPONENT_LIST, sFullUpdate );

	arrays[ 0 ]   = capnp::messageToFlatArray( messages[ 0 ] );
	arrays[ 1 ]   = capnp::messageToFlatArray( messages[ 1 ] );

	int writeSize = 0;

	for ( auto& client : gServerData.aClients )
	{
		if ( client.aState != ESV_ClientState_Connected )
			continue;

		writeSize = 0;

		for ( size_t arrayIndex = 0; arrayIndex < arrays.size(); arrayIndex++ )
		{
			int write = client.Write( arrays[ arrayIndex ].asChars().begin(), arrays[ arrayIndex ].size() * sizeof( capnp::word ) );

			// If we failed to write, disconnect them?
			if ( write == 0 )
			{
				Log_ErrorF( gLC_Server, "Failed to write network data to client, marking client as disconnected: %s\n", Net_ErrorString() );
				client.aState = ESV_ClientState_Disconnected;
			}
			else
			{
				writeSize += write;
			}
		}
	}
}
#endif


void SV_SendMessageToClient( SV_Client_t& srClient, capnp::MessageBuilder& srMessage )
{
	int write = Net_WritePacked( gServerSocket, srClient.aAddr, srMessage );
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
	NetMsgDisconnect::Builder   disconnectMsg = message.initRoot< NetMsgDisconnect >();

	disconnectMsg.setReason( "Saving chunks." );

	SV_SendMessageToClient( srClient, message );

	srClient.aState = ESV_ClientState_Disconnected;

	Log_MsgF( gLC_Server, "Disconnecting Client: \"%s\"\n", srClient.aName.c_str() );
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

		data.resize( len );

		SV_Client_t* client = SV_GetClientFromAddr( clientAddr );

		if ( !client )
		{
			SV_ConnectClient( clientAddr, data );
			continue;
		}

		// Reset the connection timer
		client->aTimeout = Game_GetCurTime() + sv_client_timeout;

		// Read the message sent from the client
		NetBufferedInputStream        inputStream( data.data(), data.size() );
		capnp::PackedMessageReader    reader( inputStream );

		// capnp::FlatArrayMessageReader reader( kj::ArrayPtr< const capnp::word >( (const capnp::word*)data.data(), data.size() ) );
		SV_ProcessClientMsg( *client, reader );
	}
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
	auto                       clientMsg = srReader.getRoot< MsgSrcClient >();

	EMsgSrcClient              msgType   = clientMsg.getType();

	// First, check types without any data attached to them
	switch ( msgType )
	{
		// Client is Disconnecting
		case EMsgSrcClient::DISCONNECT:
		{
			srClient.aState = ESV_ClientState_Disconnected;
			return;
		}

		case EMsgSrcClient::FULL_UPDATE:
		{
			// They want a full update, add them to the full update list
			gServerData.aClientsFullUpdate.push_back( &srClient );
			return;
		}

		default:
			break;
	}

	auto msgData = clientMsg.getData();

	Assert( msgData.size() );

	if ( !msgData.size() )
	{
		Log_ErrorF( gLC_Server, "Invalid Message with no data attached - %ud\n", msgType );
		return;
	}

	NetBufferedInputStream     inputStream( (char*)msgData.begin(), msgData.size() );
	capnp::PackedMessageReader dataReader( inputStream );

	switch ( msgType )
	{
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
			Log_WarnF( gLC_Server, "Unknown Message Type from Client: %ud\n", msgType );
			break;
	}
}


void SV_ConnectClientFinish( SV_Client_t& srClient )
{
	srClient.aState = ESV_ClientState_Connected;

	gServerData.aClientsConnecting.erase( &srClient );
	
	// They just got here, so send them a full update
	gServerData.aClientsFullUpdate.push_back( &srClient );

	Log_MsgF( gLC_Server, "Client Connected: \"%s\"\n", srClient.aName.c_str() );

	// Spawn the player in!
	GetPlayers()->Spawn( srClient.aEntity );

	// Reset the connection timer
	srClient.aTimeout = Game_GetCurTime() + sv_client_timeout;
}


void SV_ConnectClient( ch_sockaddr& srAddr, ChVector< char >& srData )
{
	// Get Client Info
	NetBufferedInputStream     inputStream( srData.data(), srData.size() );
	capnp::PackedMessageReader reader( inputStream );
	NetMsgClientInfo::Reader   clientInfoRead = reader.getRoot< NetMsgClientInfo >();

	SV_Client_t&               client         = gServerData.aClients.emplace_back();
	client.aName                              = clientInfoRead.getName();
	client.aAddr                              = srAddr;
	client.aState                             = ESV_ClientState_Connecting;

	Log_MsgF( gLC_Server, "Connecting Client: \"%s\"\n", client.aName.c_str() );

	// Make an entity for them
	client.aEntity      = GetEntitySystem()->CreateEntity();

	// Add the playerInfo Component
	GetEntitySystem()->AddComponent( client.aEntity, "playerInfo" );

	// Send them the server info and new port
	capnp::MallocMessageBuilder message;
	SV_BuildServerInfo( message );

	NetMsgServerInfo::Builder serverInfo = message.getRoot< NetMsgServerInfo >();

	serverInfo.setPlayerEntityId( client.aEntity );

	// send them this information on the listen socket, and with the port, the client and switch to that one for their connection
	int write = Net_WritePacked( gServerSocket, srAddr, message );

	gServerData.aClientsConnecting.push_back( &client );
}


// --------------------------------------------------------------------
// Helper Functions


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


Entity SV_GetPlayerEntFromIndex( size_t sIndex )
{
	if ( gServerData.aClients.empty() || sIndex < gServerData.aClients.size() )
		return CH_ENT_INVALID;

	return gServerData.aClients[ sIndex ].aEntity;
}

