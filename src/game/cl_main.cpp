#include "main.h"
#include "cl_main.h"
#include "game_shared.h"
#include "inputsystem.h"
#include "mapmanager.h"
#include "player.h"
#include "network/net_main.h"

#include "igui.h"
#include "iinput.h"
#include "render/irender.h"

#include "testing.h"

#include "capnproto/sidury.capnp.h"

#include <capnp/message.h>
#include <capnp/serialize-packed.h>

//
// The Client, always running unless a dedicated server
// 

// 
// IDEA:
// - Steam Leaderboard data:
//   - how many servers you connected to
//   - how many different maps you loaded
//   - how many bytes you've downloaded from servers in total
//   - how many times you've launched the game
//   

LOG_REGISTER_CHANNEL2( Client, LogColor::White );

static Socket_t                   gClientSocket = CH_INVALID_SOCKET;
ch_sockaddr                       gClientAddr;

EClientState                      gClientState = EClientState_Idle;
CL_ServerData_t                   gClientServerData;

// How much time we have until we give up connecting if the server doesn't respond anymore
static double                     gClientConnectTimeout = 0.f;
static float                      gClientTimeout        = 0.f;
static bool                       gClientMenuShown      = true;

// Console Commands to send to the server to process, like noclip
static std::vector< std::string > gCommandsToSend;
UserCmd_t                         gClientUserCmd{};

extern Entity                     gLocalPlayer;

// NEW_CVAR_FLAG( CVARF_CLIENT );


CONVAR( cl_connect_timeout_duration, 30.f, "How long we will wait for the server to send us connection information" );
CONVAR( cl_timeout_duration, 120.f, "How long we will wait for the server to start responding again before disconnecting" );
CONVAR( cl_timeout_threshold, 4.f, "If the server doesn't send anything after this amount of time, show the connection problem message" );

CONVAR( in_forward, 0, CVARF( INPUT ) );
CONVAR( in_back, 0, CVARF( INPUT ) );
CONVAR( in_left, 0, CVARF( INPUT ) );
CONVAR( in_right, 0, CVARF( INPUT ) );

CONVAR( in_duck, 0, CVARF( INPUT ) );
CONVAR( in_sprint, 0, CVARF( INPUT ) );
CONVAR( in_jump, 0, CVARF( INPUT ) );
CONVAR( in_zoom, 0, CVARF( INPUT ) );
CONVAR( in_flashlight, 0, CVARF( INPUT ) );


CONVAR_CMD_EX( cl_username, "greg", CVARF_ARCHIVE, "Your Username" )
{
	if ( cl_username.GetValue().size() <= CH_MAX_USERNAME_LEN )
		return;

	Log_WarnF( gLC_Client, "Username is too long (%zd chars), max is %d chars\n", cl_username.GetValue().size(), CH_MAX_USERNAME_LEN );
	cl_username.SetValue( prevString );
}


CONCMD_VA( cl_request_full_update )
{
	CL_SendFullUpdateRequest();
}


CONCMD( connect )
{
	if ( args.empty() )
	{
		Log_Msg( gLC_Client, "Type in an IP address after the word connect\n" );
		return;
	}

	if ( Game_GetCommandSource() == ECommandSource_Server )
	{
		// the server can only tell us to connect to localhost
		if ( args[ 0 ] != "localhost" )
		{
			Log_Msg( "connect called from server?\n" );
			return;
		}
	}

	CL_Connect( args[ 0 ].data() );
}


static void CenterMouseOnScreen()
{
	int w, h;
	SDL_GetWindowSize( render->GetWindow(), &w, &h );
	SDL_WarpMouseInWindow( render->GetWindow(), w / 2, h / 2 );
}


bool CL_Init()
{
	if ( !EntitySystem::CreateClient() )
		return false;

	Phys_CreateEnv( true );

	return true;
}


void CL_Shutdown()
{
	CL_Disconnect();

	EntitySystem::DestroyClient();

	Phys_DestroyEnv( true );
}


void CL_WriteMsgData( MsgSrcClient::Builder& srBuilder, capnp::MessageBuilder& srBuilderData )
{
	NetOutputStream outputStream;
	capnp::writePackedMessage( outputStream, srBuilderData );

	auto data = srBuilder.initData( outputStream.aBuffer.size_bytes() );
	memcpy( data.begin(), outputStream.aBuffer.begin(), outputStream.aBuffer.size_bytes() );
}


void CL_Update( float frameTime )
{
	Game_SetClient( true );
	Game_SetCommandSource( ECommandSource_Server );

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
			// Try to load the map if we aren't hosting the server
			if ( !Game_IsHosting() )
			{
				// Do we have the map?
				if ( !MapManager_FindMap( gClientServerData.aMapName ) )
				{
					// Maybe one day we can download the map from the server
					CL_Disconnect( true, "Missing Map" );
					break;
				}

				// Load Map (MAKE THIS ASYNC)
				if ( !MapManager_LoadMap( gClientServerData.aMapName ) )
				{
					CL_Disconnect( true, "Failed to Load Map" );
					break;
				}
			}

			gClientState = EClientState_Connected;
			GetEntitySystem()->AddComponent( gLocalPlayer, "playerInfo" );
			break;
		}

		case EClientState_Connected:
		{
			// Process Stuff from server
			CL_GetServerMessages();

			CL_UpdateUserCmd();

			CL_GameUpdate( frameTime );

			// Send Console Commands
			CL_SendConVars();

			// Send UserCmd
			CL_SendUserCmd();

			GetPlayers()->apMove->DisplayPlayerStats( gLocalPlayer );

			// Update Entity and Component States after everything is processed
			GetEntitySystem()->UpdateStates();

			break;
		}
	}

	Game_SetCommandSource( ECommandSource_Console );
}


void CL_GameUpdate( float frameTime )
{
	CL_UpdateMenuShown();

	// Check connection timeout
	// if ( ( cl_timeout_duration - cl_timeout_threshold ) < gClientTimeout )
	if ( cl_timeout_threshold < ( gClientTimeout - cl_timeout_duration ) )
	{
		// Show Connection Warning
		// flood console lol
		Log_WarnF( gLC_Client, "CONNECTION PROBLEM - %.3f SECONDS LEFT\n", gClientTimeout );
	}

	GetEntitySystem()->UpdateSystems();

	GetPlayers()->UpdateLocalPlayer();

	TEST_CL_UpdateProtos( frameTime );

	if ( input->WindowHasFocus() && !CL_IsMenuShown() )
	{
		CenterMouseOnScreen();
	}
}


bool CL_IsMenuShown()
{
	return gClientMenuShown;
}


void CL_UpdateMenuShown()
{
	bool wasShown    = gClientMenuShown;
	gClientMenuShown = gui->IsConsoleShown();

	if ( wasShown != gClientMenuShown )
	{
		SDL_SetRelativeMouseMode( (SDL_bool)!gClientMenuShown );

		if ( gClientMenuShown )
		{
			CenterMouseOnScreen();
		}
	}
}


// =======================================================================
// Client Networking
// =======================================================================


void CL_Disconnect( bool sSendReason, const char* spReason )
{
	if ( gClientSocket != CH_INVALID_SOCKET )
	{
		if ( sSendReason )
		{
			// Tell the server we are disconnecting
			capnp::MallocMessageBuilder builder;
			auto                        root = builder.initRoot< MsgSrcClient >();
			root.setType( EMsgSrcClient::DISCONNECT );

			capnp::MallocMessageBuilder disconnectBuilder;
			auto                        msgDisconnect = disconnectBuilder.initRoot< NetMsgDisconnect >();

			msgDisconnect.setReason( spReason ? spReason : "Client Disconnect" );

			CL_WriteMsgData( root, disconnectBuilder );
			CL_WriteToServer( builder );
		}

		Net_CloseSocket( gClientSocket );
		gClientSocket = CH_INVALID_SOCKET;
	}

	memset( &gClientAddr, 0, sizeof( gClientAddr ) );

	if ( gClientState != EClientState_Idle )
		Log_Dev( 1, "Setting Client State to Idle (Disconnecting)\n" );

	gClientState          = EClientState_Idle;
	gClientConnectTimeout = 0.f;

	GetEntitySystem()->Shutdown();
}


// TODO: should only be platform specific, needs to have sockaddr abstracted
extern void Net_NetadrToSockaddr( const NetAddr_t* spNetAddr, struct sockaddr* spSockAddr );

void CL_Connect( const char* spAddress )
{
	// Make sure we are not connected to a server already
	CL_Disconnect();

	// This seems odd being here
	if ( !GetEntitySystem()->Init() )
	{
		Log_Error( gLC_Client, "Failed to Init Client Entity System\n" );
		return;
	}

	Log_MsgF( gLC_Client, "Connecting to \"%s\"\n", spAddress );

	capnp::MallocMessageBuilder message;
	NetMsgClientInfo::Builder   clientInfoBuild = message.initRoot< NetMsgClientInfo >();

	clientInfoBuild.setProtocol( CH_SIDURY_PROTOCOL );
	clientInfoBuild.setName( cl_username.GetValue().data() );

	gClientSocket     = Net_OpenSocket( "0" );
	NetAddr_t netAddr = Net_GetNetAddrFromString( spAddress );

	Net_NetadrToSockaddr( &netAddr, (struct sockaddr*)&gClientAddr );

	int connectRet = Net_Connect( gClientSocket, gClientAddr );

	if ( connectRet != 0 )
		return;

	int write = Net_WritePacked( gClientSocket, gClientAddr, message );

	if ( write > 0 )
	{
		// Continue connecting in CL_Update()
		gClientState          = EClientState_RecvServerInfo;
		gClientConnectTimeout = Game_GetCurTime() + cl_connect_timeout_duration;
	}
	else
	{
		CL_Disconnect();
	}
}


int CL_WriteToServer( capnp::MessageBuilder& srBuilder )
{
	return Net_WritePacked( gClientSocket, gClientAddr, srBuilder );
}


bool CL_RecvServerInfo()
{
	ChVector< char > data( 8192 );
	int              len = Net_Read( gClientSocket, data.data(), data.size(), &gClientAddr );

	if ( len == 0 )
	{
		// NOTE: this might get hit, we need some sort of retry thing
		Log_Warn( gLC_Client, "No Server Info\n" );

		if ( gClientConnectTimeout < Game_GetCurTime() )
			return false;

		// keep waiting i guess?
		return true;
	}
	else if ( len < 0 )
	{
		CL_Disconnect( false );
		return false;
	}

	Log_Msg( gLC_Client, "Receiving Server Info\n" );

	NetBufferedInputStream        inputStream( (char*)data.begin(), len );
	capnp::PackedMessageReader    reader( inputStream );

	NetMsgServerInfo::Reader      serverInfoMsg = reader.getRoot< NetMsgServerInfo >();

	CL_HandleMsg_ServerInfo( serverInfoMsg );
	gLocalPlayer   = serverInfoMsg.getClientEntityId();
	gClientState   = EClientState_Connecting;

	// Reset the connection timer
	gClientTimeout = cl_timeout_duration;

	return true;
}


bool CL_SendConVarIfClient( std::string_view sName, const std::vector< std::string >& srArgs )
{
	if ( Game_ProcessingClient() )
	{
		// Send the command to the server
		CL_SendConVar( sName, srArgs );
		return true;
	}

	return false;
}


void CL_SendConVar( std::string_view sName, const std::vector< std::string >& srArgs )
{
	// We have to rebuild this command to a normal string, fun
	// Allocate a command to send with the command name
	std::string& newCmd = gCommandsToSend.emplace_back( sName.data() );

	// Add the command arguments
	for ( const std::string& arg : srArgs )
	{
		newCmd += " " + arg;
	}
}


void CL_SendConVars()
{
	// Only send if we actually have commands to send
	if ( gCommandsToSend.empty() )
		return;

	capnp::MallocMessageBuilder builder;
	auto                        root = builder.initRoot< MsgSrcClient >();
	root.setType( EMsgSrcClient::CON_VAR );

	capnp::MallocMessageBuilder convarBuilder;
	auto                        convarMsg = convarBuilder.initRoot< NetMsgConVar >();

	std::string command;

	// Join it all into one string
	for ( auto& cmd : gCommandsToSend )
		command += cmd + ";";

	// Clear it
	gCommandsToSend.clear();

	convarMsg.setCommand( command );

	CL_WriteMsgData( root, convarBuilder );
	CL_WriteToServer( builder );
}


void CL_UpdateUserCmd()
{
	// Get the camera component from the local player, and get the angles from it
	CCamera* camera = GetCamera( gLocalPlayer );

	Assert( camera );

	// Reset Values
	gClientUserCmd.aButtons    = 0;
	gClientUserCmd.aFlashlight = false;

	if ( camera )
		gClientUserCmd.aAng = camera->aTransform.Get().aAng;

	// Don't update buttons if the menu is shown
	if ( CL_IsMenuShown() )
	 	return;

	if ( in_duck )
		gClientUserCmd.aButtons |= EBtnInput_Duck;

	else if ( in_sprint )
		gClientUserCmd.aButtons |= EBtnInput_Sprint;

	if ( in_forward ) gClientUserCmd.aButtons |= EBtnInput_Forward;
	if ( in_back )    gClientUserCmd.aButtons |= EBtnInput_Back;
	if ( in_left )    gClientUserCmd.aButtons |= EBtnInput_Left;
	if ( in_right )   gClientUserCmd.aButtons |= EBtnInput_Right;
	if ( in_jump )    gClientUserCmd.aButtons |= EBtnInput_Jump;
	if ( in_zoom )    gClientUserCmd.aButtons |= EBtnInput_Zoom;
	
	if ( in_flashlight == IN_CVAR_JUST_PRESSED )
		gClientUserCmd.aFlashlight = true;
}


void CL_BuildUserCmd( capnp::MessageBuilder& srBuilder )
{
	auto builder = srBuilder.initRoot< NetMsgUserCmd >();
	
	auto ang     = builder.initAngles();
	NetHelper_WriteVec3( &ang, gClientUserCmd.aAng );

	builder.setButtons( gClientUserCmd.aButtons );
	builder.setFlashlight( gClientUserCmd.aFlashlight );
	builder.setMoveType( static_cast< EPlayerMoveType >( gClientUserCmd.aMoveType ) );
}


void CL_SendUserCmd()
{
	capnp::MallocMessageBuilder builder;
	auto                        root = builder.initRoot< MsgSrcClient >();
	root.setType( EMsgSrcClient::USER_CMD );

	capnp::MallocMessageBuilder userCmdBuilder;
	CL_BuildUserCmd( userCmdBuilder );

	CL_WriteMsgData( root, userCmdBuilder );
	CL_WriteToServer( builder );
}


void CL_SendFullUpdateRequest()
{
	capnp::MallocMessageBuilder builder;
	auto                        root = builder.initRoot< MsgSrcClient >();
	root.setType( EMsgSrcClient::FULL_UPDATE );
	CL_WriteToServer( builder );
}


void CL_SendMessageToServer( EMsgSrcClient sSrcType )
{
}


void CL_HandleMsg_ServerInfo( NetMsgServerInfo::Reader& srReader )
{
	gClientServerData.aName        = srReader.getName();
	gClientServerData.aMapName     = srReader.getMapName();
	gClientServerData.aClientCount = srReader.getClientCount();
	gClientServerData.aMaxClients  = srReader.getMaxClients();
}


// IDEA: what if you had an internal translation list or whatever of server to client entity id's
// The client entity ID's may be different thanks to client only entities
// and having an internal system to convert the entity id to what we store it on the client
// would completely resolve any potential entity id conflicts
void CL_HandleMsg_EntityList( NetMsgEntityUpdates::Reader& srReader )
{
	PROF_SCOPE();

	for ( const NetMsgEntityUpdate::Reader& entityUpdate : srReader.getUpdateList() )
	{
		Entity entId  = entityUpdate.getId();
		Entity entity = CH_ENT_INVALID;

		if ( entityUpdate.getDestroyed() )
		{
			GetEntitySystem()->DeleteEntity( entId );
			continue;
		}
		else
		{
			if ( !GetEntitySystem()->EntityExists( entId ) )
			{
				// Log_Warn( gLC_Client, "Missed message from server to create entity\n" );

				entity = GetEntitySystem()->CreateEntityFromServer( entId );

				if ( entity == CH_ENT_INVALID )
				{
					Log_Warn( gLC_Client, "Failed to create client entity from server\n" );
					continue;
				}
			}
		}
	}
}


void CL_HandleMsg_ComponentList( NetMsgComponentUpdates::Reader& srReader )
{
	PROF_SCOPE();

	for ( const NetMsgComponentUpdate::Reader& componentUpdate : srReader.getUpdateList() )
	{
		if ( !componentUpdate.hasName() )
			continue;

		const char* componentName = componentUpdate.getName().cStr();

		EntityComponentPool* pool = GetEntitySystem()->GetComponentPool( componentName );

		AssertMsg( pool, "Failed to find component pool" );

		if ( !pool )
		{
			Log_ErrorF( "Failed to find component pool for component: \"%s\"\n", componentName );
			continue;
		}

		EntComponentData_t* regData = pool->GetRegistryData();

		AssertMsg( regData, "Failed to find component registry data" );
		
		for ( const NetMsgComponentUpdate::Component::Reader& componentRead : componentUpdate.getComponents() )
		{
			// Get the Entity and Make sure it exists
			Entity entId         = componentRead.getId();
			Entity entity        = CH_ENT_INVALID;

			if ( !GetEntitySystem()->EntityExists( entId ) )
			{
				Log_Error( gLC_Client, "Failed to find entity while updating components from server\n" );
				continue;
			}

			entity = entId;

			// Check the component state, do we need to remove it, or add it to the entity?
			if ( componentRead.getDestroyed() )
			{
				// We can just remove the component right now, no need to queue it,
				// as this is before all client game processing
				pool->Remove( entity );
				continue;
			}

			void* componentData = GetEntitySystem()->GetComponent( entity, componentName );

			if ( !componentData )
			{
				// Create the component
				componentData = GetEntitySystem()->AddComponent( entity, componentName );

				if ( componentData == nullptr )
				{
					Log_ErrorF( "Failed to create component\n" );
					continue;
				}
			}

			// Now, update component data
			if ( !regData->apRead )
				continue;

			// NOTE: i could try to check if it's predicted here and get rid of aOverrideClient
			if ( !regData->aOverrideClient && entity == gLocalPlayer )
				continue;

			if ( componentRead.hasValues() )
			{
				auto                       values = componentRead.getValues();
				NetBufferedInputStream     inputStream( (char*)values.begin(), values.size() );
				capnp::PackedMessageReader reader( inputStream );

				regData->apRead( reader, componentData );

				Log_DevF( gLC_Client, 2, "Parsed component data for entity \"%zd\" - \"%s\"\n", entity, componentName );

				// Tell the Component System this entity's component updated
				IEntityComponentSystem* system = GetEntitySystem()->GetComponentSystem( regData->apName );

				if ( system )
				{
					system->ComponentUpdated( entity, componentData );
				}
			}
		}
	}
}


void CL_GetServerMessages()
{
	PROF_SCOPE();

	while ( true )
	{
		// TODO: SETUP FRAGMENT COMPRESSION !!!!!!!!
		ChVector< char > data( 819200 );
		int              len = Net_Read( gClientSocket, data.data(), data.size(), &gClientAddr );

		if ( len <= 0 )
		{
			gClientTimeout -= gFrameTime;

			// The server hasn't sent anything in a while, so just disconnect
			if ( gClientTimeout < 0.0 )
			{
				Log_Msg( gLC_Client, "Disconnecting From Server - Timeout period expired\n" );
				CL_Disconnect();
			}

			return;
		}

		data.resize( len );

		// Reset the connection timer
		gClientTimeout = cl_timeout_duration;

		// Read the message sent from the client
		NetBufferedInputStream     inputStream( (char*)data.begin(), data.size() );
		capnp::PackedMessageReader reader( inputStream );

		auto                       serverMsg = reader.getRoot< MsgSrcServer >();
		auto                       msgType   = serverMsg.getType();

		Assert( msgType < EMsgSrcServer::COUNT );
		Assert( msgType >= EMsgSrcServer::DISCONNECT );

		if ( msgType >= EMsgSrcServer::COUNT )
		{
			Log_WarnF( gLC_Client, "Unknown Message Type from Server: %zd\n", msgType );
			continue;
		}

		// Check Messages without message data first
		switch ( msgType )
		{
			// Client is Disconnecting
			case EMsgSrcServer::DISCONNECT:
			{
				CL_Disconnect();
				return;
			}

			default:
				break;
		}

		// Now check messages with message data
		auto                       msgData = serverMsg.getData();

		Assert( msgData.size() );

		if ( !msgData.size() )
		{
			Log_WarnF( gLC_Client, "Received Server Message Without Data: %s\n", SV_MsgToString( msgType ) );
			continue;
		}

		NetBufferedInputStream     inputStreamMsg( (char*)msgData.begin(), msgData.size() );
		capnp::PackedMessageReader dataReader( inputStreamMsg );

		switch ( msgType )
		{
			case EMsgSrcServer::SERVER_INFO:
			{
				auto msgServerInfo = dataReader.getRoot< NetMsgServerInfo >();
				CL_HandleMsg_ServerInfo( msgServerInfo );
				break;
			}

			case EMsgSrcServer::CON_VAR:
			{
				auto msgConVar = dataReader.getRoot< NetMsgConVar >();
				Game_ExecCommandsSafe( ECommandSource_Server, msgConVar.getCommand().cStr() );
				break;
			}

			case EMsgSrcServer::COMPONENT_LIST:
			{
				auto msg = dataReader.getRoot< NetMsgComponentUpdates >();
				CL_HandleMsg_ComponentList( msg );
				break;
			}

			case EMsgSrcServer::ENTITY_LIST:
			{
				auto msg = dataReader.getRoot< NetMsgEntityUpdates >();
				CL_HandleMsg_EntityList( msg );
				break;
			}

			default:
				Log_WarnF( gLC_Client, "Unknown Message Type from Server: %s\n", SV_MsgToString( msgType ) );
				break;
		}
	}
}


void CL_PrintStatus()
{
	if ( gClientState == EClientState_Connected )
	{
		size_t playerCount = GetPlayers()->aEntities.size();

		Log_MsgF( "Connected To %s\n", Net_AddrToString( gClientAddr ) );

		Log_MsgF( "Map: %s\n", MapManager_GetMapPath().data() );
		Log_MsgF( "%zd Players Currently on Server\n", playerCount );
	}
	else
	{
		Log_Msg( "Not Connected to or is Hosting any Server\n" );
	}
}


CONCMD( status_cl )
{
	CL_PrintStatus();
}

