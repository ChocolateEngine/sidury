#include "main.h"
#include "game_shared.h"
#include "cl_main.h"
#include "inputsystem.h"
#include "mapmanager.h"
#include "player.h"
#include "steam.h"
#include "network/net_main.h"

#include "flatbuffers/sidury_generated.h"

#include "imgui/imgui.h"
#include "igui.h"
#include "iinput.h"
#include "render/irender.h"

#include "testing.h"

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
//   - how many workshop addons you've subscribed to in total
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

// AAAAAAAAAAAAAA
static bool                       gClientWait_EntityList            = false;
static bool                       gClientWait_ComponentList         = false;
static bool                       gClientWait_ServerInfo            = false;
// static bool                       gClientWait_ComponentRegistryInfo = false;

// Console Commands to send to the server to process, like noclip
static std::vector< std::string > gCommandsToSend;
UserCmd_t                         gClientUserCmd{};

std::vector< CL_Client_t >        gClClients;

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


CONVAR_CMD_EX( cl_username_use_steam, 1, CVARF_ARCHIVE, "Use username from steam instead of what cl_username contains" )
{
	// TODO: callback to send a username change to a server if we are connected to one
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

	// Load Our Avatar
	if ( IsSteamLoaded() )
	{
		steam->RequestAvatarImage( ESteamAvatarSize_Large, steam->GetSteamID() );
	}

	return true;
}


void CL_Shutdown()
{
	CL_Disconnect();

	EntitySystem::DestroyClient();

	Phys_DestroyEnv( true );
}


void CL_WriteMsgData( flatbuffers::FlatBufferBuilder& srRootBuilder, flatbuffers::FlatBufferBuilder& srDataBuffer, EMsgSrc_Client sType )
{
	PROF_SCOPE();

	auto                 vector = srRootBuilder.CreateVector( srDataBuffer.GetBufferPointer(), srDataBuffer.GetSize() );

	MsgSrc_ClientBuilder root( srRootBuilder );
	root.add_type( sType );
	root.add_data( vector );
	srRootBuilder.Finish( root.Finish() );
}


// Same as CL_WriteMsgData, but also sends it to the server
void CL_WriteMsgDataToServer( flatbuffers::FlatBufferBuilder& srDataBuffer, EMsgSrc_Client sType )
{
	PROF_SCOPE();

	flatbuffers::FlatBufferBuilder builder;
	auto                           vector = builder.CreateVector( srDataBuffer.GetBufferPointer(), srDataBuffer.GetSize() );

	MsgSrc_ClientBuilder           root( builder );
	root.add_type( sType );
	root.add_data( vector );
	builder.Finish( root.Finish() );

	CL_WriteToServer( builder );
}


void CL_Update( float frameTime )
{
	PROF_SCOPE();

	Game_SetClient( true );
	Game_SetCommandSource( ECommandSource_Server );

	switch ( gClientState )
	{
		default:
		case EClientState_Idle:
			break;

		case EClientState_WaitForAccept:
		case EClientState_WaitForFullUpdate:
		{
			// Recieve Server Info first
			CL_GetServerMessages();
			break;
		}

		case EClientState_Connecting:
		{
			CL_GetServerMessages();

			// I HATE THIS
			if ( gClientWait_EntityList && gClientWait_ComponentList && gClientWait_ServerInfo )
			{
				// Try to load the map if we aren't hosting the server
				if ( !Game_IsHosting() )
				{
					// Do we have the map?
					if ( !MapManager_FindMap( gClientServerData.aMapName ) )
					{
						// Maybe one day we can download the map from the server
						Log_ErrorF( gLC_Client, "Failed to Find Map: \"%s\"\n", gClientServerData.aMapName.c_str() );
						CL_Disconnect( true, "Missing Map" );
						break;
					}

					// Load Map (MAKE THIS ASYNC)
					if ( !MapManager_LoadMap( gClientServerData.aMapName ) )
					{
						Log_ErrorF( gLC_Client, "Failed to Load Map: \"%s\"\n", gClientServerData.aMapName.c_str() );
						CL_Disconnect( true, "Failed to Load Map" );
						break;
					}
				}

				gClientState = EClientState_Connected;

				// Tell the server we've connected
				flatbuffers::FlatBufferBuilder builder;
				MsgSrc_ClientBuilder           root( builder );

				root.add_type( EMsgSrc_Client_ConnectFinish );
				builder.Finish( root.Finish() );
				CL_WriteToServer( builder );
			}

			break;
		}

		case EClientState_Connected:
		{
			// Process Stuff from server
			CL_GetServerMessages();

			if ( !Game_IsPaused() && input->WindowHasFocus() && !CL_IsMenuShown() )
				GetPlayers()->DoMouseLook( gLocalPlayer );

			CL_UpdateUserCmd();

			CL_GameUpdate( frameTime );

			// Send Console Commands
			CL_SendConVars();

			// Send UserCmd
			CL_SendUserCmd();

			GetPlayers()->apMove->DisplayPlayerStats( gLocalPlayer );

			break;
		}
	}

	// Update Entity and Component States
	GetEntitySystem()->UpdateStates();

	if ( CL_IsMenuShown() )
	{
		CL_DrawMainMenu();
	}

	Game_SetCommandSource( ECommandSource_Console );
}


void CL_GameUpdate( float frameTime )
{
	PROF_SCOPE();

	CL_UpdateMenuShown();

	// Check connection timeout
	if ( ( cl_timeout_duration - cl_timeout_threshold ) > ( gClientTimeout ) )
	{
		// Show Connection Warning
		gui->DebugMessage( "CONNECTION PROBLEM - %.3f SECONDS LEFT\n", gClientTimeout );
	}

	GetEntitySystem()->InitCreatedComponents();

	GetEntitySystem()->UpdateSystems();

	GetPlayers()->UpdateLocalPlayer();

	if ( input->WindowHasFocus() && !CL_IsMenuShown() )
	{
		CenterMouseOnScreen();
	}
}


const char* CL_GetUserName()
{
	if ( cl_username_use_steam && IsSteamLoaded() )
	{
		const char* steamProfileName = steam->GetPersonaName();

		if ( steamProfileName )
			return steamProfileName;
	}

	return cl_username;
}


void CL_SetClientSteamAvatar( SteamID64_t sSteamID, ESteamAvatarSize sSize, Handle sAvatar )
{
	for ( CL_Client_t& client : gClClients )
	{
		if ( client.aSteamID != sSteamID )
			continue;
		
		switch ( sSize )
		{
			case ESteamAvatarSize_Large:
				client.aAvatarLarge = sAvatar;
				break;

			case ESteamAvatarSize_Medium:
				client.aAvatarMedium = sAvatar;
				break;

			case ESteamAvatarSize_Small:
				client.aAvatarSmall = sAvatar;
				break;
		}

		break;
	}
}


Handle CL_GetClientSteamAvatar( SteamID64_t sSteamID, ESteamAvatarSize sSize )
{
	for ( CL_Client_t& client : gClClients )
	{
		if ( client.aSteamID != sSteamID )
			continue;
		
		switch ( sSize )
		{
			case ESteamAvatarSize_Large:
				return client.aAvatarLarge;

			case ESteamAvatarSize_Medium:
				return client.aAvatarMedium;

			case ESteamAvatarSize_Small:
				return client.aAvatarSmall;
		}
	}

	return InvalidHandle;
}


Handle CL_PickClientSteamAvatar( SteamID64_t sSteamID, int sWidth )
{
	for ( CL_Client_t& client : gClClients )
	{
		if ( client.aSteamID != sSteamID )
			continue;

		if ( sWidth < 64 )
			return client.aAvatarSmall;

		if ( sWidth < 184 )
			return client.aAvatarMedium;

		else
			return client.aAvatarLarge;
	}

	return InvalidHandle;
}


// =======================================================================
// Client Networking
// =======================================================================


void CL_Disconnect( bool sSendReason, const char* spReason )
{
	if ( gClientSocket != CH_INVALID_SOCKET )
	{
		// if ( sSendReason )
		// {
		// 	// Tell the server we are disconnecting
		// 	flatbuffers::FlatBufferBuilder builder;
		// 	auto                        root = builder.initRoot< MsgSrcClient >();
		// 	root.setType( EMsgSrcClient::DISCONNECT );
		// 
		// 	flatbuffers::FlatBufferBuilder disconnectBuilder;
		// 	auto                        msgDisconnect = disconnectBuilder.initRoot< NetMsgDisconnect >();
		// 
		// 	msgDisconnect.setReason( spReason ? spReason : "Client Disconnect" );
		// 
		// 	CL_WriteMsgData( root, disconnectBuilder );
		// 	CL_WriteToServer( builder );
		// }

		Net_CloseSocket( gClientSocket );
		gClientSocket = CH_INVALID_SOCKET;
	}

	memset( &gClientAddr, 0, sizeof( gClientAddr ) );

	if ( gClientState != EClientState_Idle )
	{
		if ( spReason )
			Log_DevF( 1, "Setting Client State to Idle (Disconnecting) - %s\n", spReason );
		else
			Log_Dev( 1, "Setting Client State to Idle (Disconnecting)\n" );
	}

	gClientState              = EClientState_Idle;
	gClientConnectTimeout     = 0.f;

	gClientWait_EntityList    = false;
	gClientWait_ComponentList = false;
	gClientWait_ServerInfo    = false;

	if ( GetEntitySystem() )
		GetEntitySystem()->Shutdown();
}


// TODO: should only be platform specific, needs to have sockaddr abstracted
extern void Net_NetadrToSockaddr( const NetAddr_t* spNetAddr, struct sockaddr* spSockAddr );

void CL_Connect( const char* spAddress )
{
	PROF_SCOPE();

	// Make sure we are not connected to a server already
	CL_Disconnect();

	// This seems odd being here
	if ( !GetEntitySystem()->Init() )
	{
		Log_Error( gLC_Client, "Failed to Init Client Entity System\n" );
		return;
	}

	Log_MsgF( gLC_Client, "Connecting to \"%s\"\n", spAddress );

	flatbuffers::FlatBufferBuilder builder( 1024 );
	auto                           msgClientConnect = CreateNetMsg_ClientConnect( builder, ESiduryProtocolVer_Value );

	// flatbuffers::FlatBufferBuilder message;
	// NetMsgClientInfo::Builder   clientInfoBuild = message.initRoot< NetMsgClientInfo >();
	// 
	// clientInfoBuild.setProtocol( CH_SIDURY_PROTOCOL );
	// clientInfoBuild.setName( CL_GetUserName() );
	// clientInfoBuild.setSteamID( IsSteamLoaded() ? steam->GetSteamID() : 0 );

	gClientSocket     = Net_OpenSocket( "0" );
	NetAddr_t netAddr = Net_GetNetAddrFromString( spAddress );

	Net_NetadrToSockaddr( &netAddr, (struct sockaddr*)&gClientAddr );

	int connectRet = Net_Connect( gClientSocket, gClientAddr );

	if ( connectRet != 0 )
		return;

	builder.Finish( msgClientConnect );
	int write = Net_WriteFlatBuffer( gClientSocket, gClientAddr, builder );

	if ( write > 0 )
	{
		// Continue connecting in CL_Update()
		gClientState          = EClientState_WaitForAccept;
		gClientTimeout        = cl_connect_timeout_duration;
		gClientConnectTimeout = Game_GetCurTime() + cl_connect_timeout_duration;
	}
	else
	{
		CL_Disconnect();
	}
}


int CL_WriteToServer( flatbuffers::FlatBufferBuilder& srBuilder )
{
	return Net_WriteFlatBuffer( gClientSocket, gClientAddr, srBuilder );
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
	PROF_SCOPE();

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
	PROF_SCOPE();

	// Only send if we actually have commands to send
	if ( gCommandsToSend.empty() )
		return;

	std::string command;

	// Join it all into one string
	for ( auto& cmd : gCommandsToSend )
		command += cmd + ";";

	// Clear it
	gCommandsToSend.clear();

	flatbuffers::FlatBufferBuilder convarBuffer;
	auto msg = CreateNetMsg_ConVar( convarBuffer, convarBuffer.CreateString( command ) );
	convarBuffer.Finish( msg );
	CL_WriteMsgDataToServer( convarBuffer, EMsgSrc_Client_ConVar );
}


void CL_UpdateUserCmd()
{
	PROF_SCOPE();

	// Get the transform component from the camera entity on the local player, and get the angles from it
	auto playerInfo = Ent_GetComponent< CPlayerInfo >( gLocalPlayer, "playerInfo" );

	if ( !playerInfo )
		return;

	auto camTransform = Ent_GetComponent< CTransform >( playerInfo->aCamera, "transform" );

	CH_ASSERT( camTransform );

	// Reset Values
	gClientUserCmd.aButtons    = 0;
	gClientUserCmd.aFlashlight = false;

	if ( camTransform )
	{
		gClientUserCmd.aAng = camTransform->aAng;
		// gClientUserCmd.aAng[ YAW ] *= -1;
	}

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


void CL_BuildUserCmd( flatbuffers::FlatBufferBuilder& srBuilder )
{
	PROF_SCOPE();

	flatbuffers::Offset< Vec3 > anglesOffset{};
	// CH_NET_WRITE_VEC3( angles, gClientUserCmd.aAng );

#if 0
	Vec3Builder vec3Build( srBuilder );
	NetHelper_WriteVec3( vec3Build, gClientUserCmd.aAng );
	anglesOffset = vec3Build.Finish();
#endif
	
	NetMsg_UserCmdBuilder builder( srBuilder );

	Vec3                  posVec( gClientUserCmd.aAng.x, gClientUserCmd.aAng.y, gClientUserCmd.aAng.z );
	builder.add_angles( &posVec );

	CH_NET_WRITE_OFFSET( builder, angles );
	builder.add_buttons( gClientUserCmd.aButtons );
	builder.add_flashlight( gClientUserCmd.aFlashlight );
	builder.add_move_type( static_cast< EPlayerMoveType >( gClientUserCmd.aMoveType ) );

	srBuilder.Finish( builder.Finish() );
}


void CL_SendUserCmd()
{
	PROF_SCOPE();

	flatbuffers::FlatBufferBuilder userCmdBuilder;
	CL_BuildUserCmd( userCmdBuilder );
	CL_WriteMsgDataToServer( userCmdBuilder, EMsgSrc_Client_UserCmd );
}


void CL_SendFullUpdateRequest()
{
	PROF_SCOPE();

	flatbuffers::FlatBufferBuilder builder;
	MsgSrc_ClientBuilder           root( builder );

	root.add_type( EMsgSrc_Client_FullUpdate );
	CL_WriteToServer( builder );
}


void CL_SendMessageToServer( EMsgSrc_Client sSrcType )
{
}


void CL_HandleMsg_ClientInfo( const NetMsg_ServerClientInfo* spMessage )
{
	PROF_SCOPE();

	if ( !spMessage )
		return;

	if ( spMessage->steam_id() == 0 )
		return;

	// Find the client by Entity ID
	CL_Client_t* spClient = nullptr;

	for ( auto client: gClClients )
	{
		if ( client.aEntity == spMessage->entity_id() )
		{
			spClient = &client;
			break;
		}
	}

	// If none is found, create one
	if ( !spClient )
		spClient = &gClClients.emplace_back();

	if ( spMessage->name() )
		spClient->aName = spMessage->name()->str();
	
	spClient->aSteamID = spMessage->steam_id();
	spClient->aEntity  = spMessage->entity_id();
}


void CL_HandleMsg_ServerConnectResponse( const NetMsg_ServerConnectResponse* spMsg )
{
	if ( !spMsg )
		return;

	Entity entity = spMsg->client_entity_id();
	
	if ( entity == CH_ENT_INVALID )
	{
		CL_Disconnect( false );
		return;
	}

	gLocalPlayer = entity;
	gClientState = EClientState_Connecting;

	// Send them our client info now
	flatbuffers::FlatBufferBuilder dataBuilder;
	auto                           name = dataBuilder.CreateString( CL_GetUserName() );

	NetMsg_ClientInfoBuilder       clientInfo( dataBuilder );

	clientInfo.add_name( name );
	clientInfo.add_steam_id( IsSteamLoaded() ? steam->GetSteamID() : 0 );
	dataBuilder.Finish( clientInfo.Finish() );

	CL_WriteMsgDataToServer( dataBuilder, EMsgSrc_Client_ClientInfo );
}


void CL_HandleMsg_ServerInfo( const NetMsg_ServerInfo* spMsg )
{
	if ( !spMsg )
		return;

	gClientWait_ServerInfo = true;

	if ( spMsg->name() )
		gClientServerData.aName = spMsg->name()->str();

	if ( spMsg->map_name() )
		gClientServerData.aMapName = spMsg->map_name()->str();

	gClientServerData.aClientCount = spMsg->client_count();
	gClientServerData.aMaxClients  = spMsg->max_clients();
}


template< typename T >
inline bool CL_VerifyMsg( EMsgSrc_Server sMsgType, flatbuffers::Verifier& srVerifier, const T* spMsg )
{
	if ( !spMsg->Verify( srVerifier ) )
	{
		Log_WarnF( gLC_Client, "Message Data is not Valid: %s\n", SV_MsgToString( sMsgType ) );
		return false;
	}

	return true;
}

template< typename T >
inline const T* CL_ReadMsg( EMsgSrc_Server sMsgType, flatbuffers::Verifier& srVerifier, const flatbuffers::Vector< u8 >* srMsgData )
{
	auto msg = flatbuffers::GetRoot< T >( srMsgData->data() );

	if ( !msg->Verify( srVerifier ) )
	{
		Log_WarnF( gLC_Client, "Message Data is not Valid: %s\n", SV_MsgToString( sMsgType ) );
		return nullptr;
	}

	return msg;
}


void CL_GetServerMessages()
{
	PROF_SCOPE();

	while ( true )
	{
		// TODO: SETUP FRAGMENT COMPRESSION !!!!!!!!
		ChVector< char > data( 81920 );
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

		// Read the message sent from the server
		auto                  serverMsg = flatbuffers::GetRoot< MsgSrc_Server >( data.begin() );
		flatbuffers::Verifier verifyMsg( (u8*)data.data(), data.size_bytes() );

		if ( !serverMsg->Verify( verifyMsg ) )
		{
			Log_Warn( gLC_Client, "Error Parsing Message from Server\n" );
			continue;
		}


		EMsgSrc_Server msgType = serverMsg->type();

		CH_ASSERT( msgType < EMsgSrc_Server_MAX );
		CH_ASSERT( msgType >= EMsgSrc_Server_MIN );

		if ( msgType >= EMsgSrc_Server_MAX )
		{
			Log_WarnF( gLC_Client, "Unknown Message Type from Server: %zd\n", msgType );
			continue;
		}

		// Check Messages without message data first
		// switch ( msgType )
		// {
		// 	// Server is Disconnecting Us, either because it's shutting down or we are kicked, etc.
		// 	// TODO:
		// 	case EMsgSrcServer::DISCONNECT:
		// 	{
		// 		CL_Disconnect();
		// 		return;
		// 	}
		// 
		// 	default:
		// 		break;
		// }

		// Now check messages with message data
		auto msgData = serverMsg->data();

		if ( !msgData || !msgData->size() )
		{
			// Must be one of these messages to have a chance to contain no data
			switch ( msgType )
			{
				case EMsgSrc_Server_Disconnect:
				case EMsgSrc_Server_ConVar:
					continue;

				default:
					Log_WarnF( gLC_Client, "Received Server Message Without Data: %s\n", SV_MsgToString( msgType ) );
					continue;
			}

			continue;
		}

		flatbuffers::Verifier msgDataVerify( msgData->data(), msgData->size() );

		switch ( msgType )
		{
			case EMsgSrc_Server_Disconnect:
			{
				//auto msgDisconnect = dataReader.getRoot< NetMsgDisconnect >();
				//Log_MsgF( gLC_Client, "Disconnected from server: %s\n", msgDisconnect.getReason().cStr() );
				Log_Msg( gLC_Client, "Disconnected from server: \n" );
				CL_Disconnect( false );
				return;
			}

			case EMsgSrc_Server_ConnectResponse:
			{
				if ( auto msg = CL_ReadMsg< NetMsg_ServerConnectResponse >( msgType, msgDataVerify, msgData ) )
					CL_HandleMsg_ServerConnectResponse( msg );
				break;
			}

			case EMsgSrc_Server_ClientInfo:
			{
				if ( auto msg = CL_ReadMsg< NetMsg_ServerClientInfo >( msgType, msgDataVerify, msgData ) )
					CL_HandleMsg_ClientInfo( msg );
				break;
			}
			
			case EMsgSrc_Server_ServerInfo:
			{
				if ( auto msg = CL_ReadMsg< NetMsg_ServerInfo >( msgType, msgDataVerify, msgData ) )
					CL_HandleMsg_ServerInfo( msg );
				break;
			}

			case EMsgSrc_Server_ConVar:
			{
				auto msg = flatbuffers::GetRoot< NetMsg_ConVar >( msgData->data() );
				if ( CL_VerifyMsg( msgType, msgDataVerify, msg ) && msg->command() )
					Game_ExecCommandsSafe( ECommandSource_Server, msg->command()->str() );

				break;
			}

			case EMsgSrc_Server_ComponentList:
			{
				if ( auto msg = CL_ReadMsg< NetMsg_ComponentUpdates >( msgType, msgDataVerify, msgData ) )
				{
					gClientWait_ComponentList = true;
					GetEntitySystem()->ReadComponentUpdates( msg );
				}
				break;
			}

			case EMsgSrc_Server_EntityList:
			{
				if ( auto msg = CL_ReadMsg< NetMsg_EntityUpdates >( msgType, msgDataVerify, msgData ) )
				{
					gClientWait_EntityList = true;
					GetEntitySystem()->ReadEntityUpdates( msg );
				}
				break;
			}

			case EMsgSrc_Server_Paused:
			{
				if ( auto msg = CL_ReadMsg< NetMsg_Paused >( msgType, msgDataVerify, msgData ) )
				{
					Game_SetPaused( msg->paused() );
					audio->SetPaused( msg->paused() );
				}
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


// =======================================================================
// Main Menu
// =======================================================================


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


ConVarRef snd_volume( "snd_volume" );


void CL_DrawMainMenu()
{
	PROF_SCOPE();

	if ( !ImGui::Begin( "Chocolate Engine - Sidury" ) )
	{
		ImGui::End();
		return;
	}

	if ( ImGui::Button( "connect localhost" ) )
	{
		Con_QueueCommand( "connect localhost" );
	}

	const std::vector< std::string >& mapList    = MapManager_GetMapList();

	ImGui::Separator();

	const ImVec2      itemSize   = ImGui::GetItemRectSize();
	const float       textHeight = ImGui::GetTextLineHeight();
	const ImGuiStyle& style      = ImGui::GetStyle();

	// Quick Map List
	{
		// ImGui::Text( "Quick Map List" );

		ImVec2 mapChildSize = itemSize;
		mapChildSize.x -= ( style.FramePadding.x + style.ItemInnerSpacing.x ) * 2;
		mapChildSize.y += ( style.FramePadding.y + style.ItemInnerSpacing.y );
		mapChildSize.y += textHeight * 8.f;

		if ( ImGui::BeginChild( "Quick Map List", mapChildSize, true ) )
		{
			for ( const std::string& map : mapList )
			{
				if ( ImGui::Selectable( map.c_str() ) )
				{
					Con_QueueCommand( vstring( "map \"%s\"", map.c_str() ) );
				}
			}
		}

		ImGui::EndChild();
	}

	// Quick Settings
	{
		ImVec2 childSize = itemSize;
		childSize.x -= ( style.FramePadding.x + style.ItemInnerSpacing.x ) * 2;
		childSize.y += ( style.FramePadding.y + style.ItemInnerSpacing.y );
		childSize.y += textHeight * 2.f;
	
		// if ( ImGui::BeginChild( "Quick Settings", childSize, true ) )
		if ( ImGui::BeginChild( "Quick Settings", {}, true ) )
		{
			float vol = snd_volume.GetFloat();
			if ( ImGui::SliderFloat( "Volume", &vol, 0.f, 1.f ) )
			{
				snd_volume.SetValue( vol );
			}
		}
	
		ImGui::EndChild();
	}

	ImGui::End();
}

