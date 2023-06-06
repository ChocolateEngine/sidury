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
static ch_sockaddr                gClientAddr;

static EClientState               gClientState = EClientState_Idle;
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

	PlayerManager::CreateClient();

	Phys_CreateEnv( true );

	return true;
}


void CL_Shutdown()
{
	CL_Disconnect();

	EntitySystem::DestroyClient();
	PlayerManager::DestroyClient();

	Phys_DestroyEnv( true );
}


void CL_WriteMsgData( MsgSrcClient::Builder& srBuilder, capnp::MessageBuilder& srBuilderData )
{
	auto array = capnp::messageToFlatArray( srBuilderData );
	auto data  = srBuilder.initData( array.size() * sizeof( capnp::word ) );

	// This is probably awful and highly inefficent
	memcpy( data.begin(), array.begin(), array.size() * sizeof( capnp::word ) );
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

			gClientState = EClientState_Connected;
			GetEntitySystem()->AddComponent( gLocalPlayer, "playerInfo" );
			break;
		}

		case EClientState_Connected:
		{
			// Process Stuff from server
			CL_GetServerMessages();

			// CL_ExecServerCommands();

			CL_UpdateUserCmd();

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
			capnp::MallocMessageBuilder builder;
			auto                        root = builder.initRoot< MsgSrcClient >();
			root.setType( EMsgSrcClient::USER_CMD );

			capnp::MallocMessageBuilder userCmdBuilder;
			CL_SendUserCmd( userCmdBuilder );

			CL_WriteMsgData( root, userCmdBuilder );
			CL_WriteToServer( builder );

			break;
		}
	}

	Game_SetCommandSource( ECommandSource_Client );
}


void CL_GameUpdate( float frameTime )
{
	CL_UpdateMenuShown();

	// Check connection timeout
	// if ( ( cl_timeout_duration - cl_timeout_threshold ) < gClientTimeout )
	if ( cl_timeout_duration < ( gClientTimeout - cl_timeout_threshold ) )
	{
		// Show Connection Warning
		// flood console lol
		Log_WarnF( gLC_Client, "CONNECTION PROBLEM - %.3f SECONDS LEFT\n", gClientTimeout );
	}

	GetPlayers()->UpdateLocalPlayer();

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

	int connectRet = Net_Connect( gClientSocket, gClientAddr );

	if ( connectRet != 0 )
		return;

	// kj::HandleOutputStream out( (HANDLE)gClientSocket );
	// capnp::writeMessage( out, message );

	auto array = capnp::messageToFlatArray( message );

	int  write = Net_Write( gClientSocket, array.asChars().begin(), array.size() * sizeof( capnp::word ), &gClientAddr );
	// int  write = Net_Write( gClientSocket, cl_username.GetValue().data(), cl_username.GetValue().size() * sizeof( char ), &sockAddr );

	// Continue connecting in CL_Update()
	gClientState          = EClientState_RecvServerInfo;
	gClientConnectTimeout = Game_GetCurTime() + cl_connect_timeout_duration;
}


int CL_WriteToServer( capnp::MessageBuilder& srBuilder )
{
	auto finalMessage = capnp::messageToFlatArray( srBuilder );
	return Net_Write( gClientSocket, finalMessage.asChars().begin(), finalMessage.size() * sizeof( capnp::word ), &gClientAddr );
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

	capnp::FlatArrayMessageReader reader( kj::ArrayPtr< const capnp::word >( (const capnp::word*)data.data(), data.size() ) );
	NetMsgServerInfo::Reader      serverInfoMsg = reader.getRoot< NetMsgServerInfo >();

	// hack, should not be part of this server info message, should be a message prior to this
	// will set up later
	// if ( serverInfoMsg.getNewPort() != -1 )
	// {
	// 	Net_SetSocketPort( gClientAddr, serverInfoMsg.getNewPort() );
	// }

	gClientServerData.aName                     = serverInfoMsg.getName();
	gClientServerData.aMapName                  = serverInfoMsg.getMapName();
	gClientState                                = EClientState_Connecting;

	gLocalPlayer                                = serverInfoMsg.getPlayerEntityId();

	return true;
}


void CL_UpdateUserCmd()
{
	// Get the camera component from the local player, and get the angles from it
	CCamera* camera = GetCamera( gLocalPlayer );

	Assert( camera );

	// Reset Values
	gClientUserCmd.aButtons    = 0;
	gClientUserCmd.aFlashlight = false;

	gClientUserCmd.aAng     = camera->aTransform.aAng;

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


void CL_SendUserCmd( capnp::MessageBuilder& srBuilder )
{
	auto builder = srBuilder.initRoot< NetMsgUserCmd >();
	
	auto ang     = builder.initAngles();
	NetHelper_WriteVec3( &ang, gClientUserCmd.aAng );

	builder.setButtons( gClientUserCmd.aButtons );
	builder.setFlashlight( gClientUserCmd.aFlashlight );
	builder.setMoveType( static_cast< EPlayerMoveType >( gClientUserCmd.aMoveType ) );
}


void CL_SendMessageToServer( EMsgSrcClient sSrcType )
{
}


void CL_HandleMsg_ServerInfo( NetMsgServerInfo::Reader& srReader )
{
}


void CL_HandleMsg_EntityList( NetMsgEntityUpdates::Reader& srReader )
{
	PROF_SCOPE();

	for ( const NetMsgEntityUpdate::Reader& entityUpdate : srReader.getUpdateList() )
	{
		Entity entId  = entityUpdate.getId();
		Entity entity = CH_ENT_INVALID;

		if ( GetEntitySystem()->EntityExists( entId ) )
		{
			entity = entId;
		}
		else
		{
			entity = GetEntitySystem()->CreateEntityFromServer( entId );

			if ( entity == CH_ENT_INVALID )
				continue;
		}

		if ( entityUpdate.getState() == NetMsgEntityUpdate::EState::CREATED )
		{
			entity = GetEntitySystem()->CreateEntityFromServer( entId );
		
			if ( entity == CH_ENT_INVALID )
				continue;
		}
		else if ( entityUpdate.getState() == NetMsgEntityUpdate::EState::DESTROYED )
		{
			GetEntitySystem()->DeleteEntity( entId );
			continue;
		}

		if ( entity == CH_ENT_INVALID )
			continue;

		for ( const NetMsgEntityUpdate::Component::Reader& componentRead : entityUpdate.getComponents() )
		{
			const char* componentName = componentRead.getName().cStr();
			void*       componentData   = nullptr;

			// if ( componentRead.getState() == NetMsgEntityUpdate::EState::DESTROYED )
			// {
			// 	GetEntitySystem()->RemoveComponent( entity, spComponentName );
			// 	continue;
			// }

			componentData               = GetEntitySystem()->GetComponent( entity, componentName );

			// else if ( componentRead.getState() == NetMsgEntityUpdate::EState::CREATED )
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

			EntityComponentPool* pool = GetEntitySystem()->GetComponentPool( componentName );

			if ( !pool )
			{
				Log_ErrorF( "Failed to find component pool for component: \"%s\"\n", componentName );
				continue;
			}

			EntComponentData_t* regData = pool->GetRegistryData();

			Assert( regData );

			// Assert( regData->apRead );

			if ( !regData->apRead )
				continue;

			if ( !regData->aOverrideClient )
				continue;

			auto values = componentRead.getValues();

			// capnp::FlatArrayMessageReader reader( kj::ArrayPtr< const capnp::word >( (const capnp::word*)values.data(), values.size() ) );
			capnp::FlatArrayMessageReader reader( kj::ArrayPtr< const capnp::word >( (const capnp::word*)values.begin(), values.size() ) );
			regData->apRead( reader, componentData );

			Log_DevF( gLC_Client, 2, "Parsed component data for entity \"%zd\" - \"%s\"\n", entity, componentName );


			// NetMsgServerInfo::Reader      serverInfoMsg = reader.getRoot< NetMsgServerInfo >();
			// 
			// capnp::MallocMessageBuilder compMessageBuilder;
			// regData->apRead( compMessageBuilder, data );
			// auto array        = capnp::messageToFlatArray( compMessageBuilder );
			// 
			// auto valueBuilder = compBuilder.initValues( array.size() * sizeof( capnp::word ) );
			// // std::copy( array.begin(), array.end(), valueBuilder.begin() );
			// memcpy( valueBuilder.begin(), array.begin(), array.size() * sizeof( capnp::word ) );
			// 
			// componentRead.getValues();
		}
	}
}


void CL_GetServerMessages()
{
	PROF_SCOPE();

	while ( true )
	{
		ChVector< char > data( 8192 );
		int              len = Net_Read( gClientSocket, data.data(), data.size(), &gClientAddr );

		if ( len <= 0 )
		{
			gClientTimeout -= gFrameTime;

			// The server hasn't sent anything in a while, so just disconnect
			if ( gClientTimeout < 0.0 )
				CL_Disconnect();

			return;
		}

		// Reset the connection timer
		gClientTimeout = cl_timeout_duration;

		// Read the message sent from the client
		capnp::FlatArrayMessageReader reader( kj::ArrayPtr< const capnp::word >( (const capnp::word*)data.data(), data.size() ) );
		auto                          serverMsg = reader.getRoot< MsgSrcServer >();

		auto                          msgType   = serverMsg.getType();
		auto                          msgData   = serverMsg.getData();

		Assert( msgType <= EMsgSrcServer::ENTITY_LIST );
		Assert( msgType >= EMsgSrcServer::DISCONNECT );

		capnp::FlatArrayMessageReader dataReader( kj::ArrayPtr< const capnp::word >( (const capnp::word*)msgData.begin(), data.size() ) );

		switch ( msgType )
		{
			// Client is Disconnecting
			case EMsgSrcServer::DISCONNECT:
			{
				CL_Disconnect();
				return;
			}

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

			case EMsgSrcServer::ENTITY_LIST:
			{
				auto msgUserCmd = dataReader.getRoot< NetMsgEntityUpdates >();
				CL_HandleMsg_EntityList( msgUserCmd );
				break;
			}

			default:
				// TODO: have a message type to string function
				Log_WarnF( gLC_Client, "Unknown Message Type from Server: %s\n", msgType );
				break;
		}
	}
}

