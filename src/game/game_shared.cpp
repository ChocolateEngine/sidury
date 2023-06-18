#include "game_shared.h"
#include "cl_main.h"
#include "sv_main.h"
#include "entity.h"
#include "player.h"
#include "mapmanager.h"

#include <capnp/message.h>
#include <capnp/serialize-packed.h>


NEW_CVAR_FLAG( CVARF_CL_EXEC );
NEW_CVAR_FLAG( CVARF_SV_EXEC );


static bool           gGameUseClient     = true;
static ECommandSource gGameCommandSource = ECommandSource_Client;

extern ch_sockaddr    gClientAddr;
extern EClientState   gClientState;


CONCMD( disconnect )
{
	CL_Disconnect();

	if ( Game_IsHosting() )
	{
		SV_StopServer();
	}
}


CONCMD( status )
{
	if ( Game_IsHosting() )
	{
		SV_PrintStatus();
	}
	else if ( Game_IsClient() )
	{
		CL_PrintStatus();
	}

	// List Player Information
}


static const char* gMsgSrcClientStr[] = {
	"Disconnect",
	"Client Info",
	"ConVar",
	"User Command",
	"Full Update",
};


static const char* gMsgSrcServerStr[] = {
	"Disconnect",
	"Server Info",
	"ConVar",
	"Component List",
	"Entity List",
	"Paused",
	"Component Registry Info",
};


static_assert( ARR_SIZE( gMsgSrcClientStr ) == static_cast< uint16_t >( EMsgSrcClient::COUNT ) );
static_assert( ARR_SIZE( gMsgSrcServerStr ) == static_cast< uint16_t >( EMsgSrcServer::COUNT ) );


// Convert a Client Source Message to String
const char* CL_MsgToString( EMsgSrcClient sMsg )
{
	Assert( sMsg < EMsgSrcClient::COUNT );

	if ( sMsg >= EMsgSrcClient::COUNT )
		return "INVALID";

	return gMsgSrcClientStr[ static_cast< uint16_t >( sMsg ) ];
}


// Convert a Server Source Message to String
const char* SV_MsgToString( EMsgSrcServer sMsg )
{
	Assert( sMsg < EMsgSrcServer::COUNT );

	if ( sMsg >= EMsgSrcServer::COUNT )
		return "INVALID";

	return gMsgSrcClientStr[ static_cast< uint16_t >( sMsg ) ];
}


bool Game_IsHosting()
{
	return gServerData.aActive;
}


bool Game_IsClient()
{
	return true;
}


bool Game_IsServer()
{
	return gServerData.aActive;
}


bool Game_ProcessingClient()
{
	return gGameUseClient;
}


bool Game_ProcessingServer()
{
	return !gGameUseClient;
}


void Game_SetClient( bool client )
{
	gGameUseClient = client;
}


ECommandSource Game_GetCommandSource()
{
	return gGameCommandSource;
}


void Game_SetCommandSource( ECommandSource sSource )
{
	gGameCommandSource = sSource;
}


void Game_ExecCommandsSafe( ECommandSource sSource, std::string_view sCommand )
{
	std::string                commandName;
	std::vector< std::string > args;

	for ( size_t i = 0; i < sCommand.size(); i++ )
	{
		commandName.clear();
		args.clear();

		Con_ParseCommandLineEx( sCommand, commandName, args, i );
		str_lower( commandName );

		ConVarBase* cvarBase = Con_GetConVarBase( commandName );

		if ( !cvarBase )
		{
			// how did this happen?
			Log_ErrorF( "Game_ExecCommandsSafe(): Failed to find command \"%s\"\n", commandName.c_str() );
			continue;
		}

		ConVarFlag_t flags = cvarBase->GetFlags();

		// if the command is from the server and we are the client, make sure they can execute it
		if ( sSource == ECommandSource_Server && Game_ProcessingClient() )
		{
			// The Convar must have one of these flags
			if ( !(flags & CVARF_SV_EXEC) && !(flags & CVARF_SERVER) && !(flags & CVARF_REPLICATED) )
			{
				Log_WarnF( "Server Tried Executing Command without flag to allow it: \"%s\"\n", commandName.c_str() );
				continue;
			}
		}

		// if the command is from the client and we are the server, make sure they can execute it
		else if ( sSource == ECommandSource_Client && Game_ProcessingServer() )
		{
			// The Convar must have this flag
			if ( !(flags & CVARF_CL_EXEC) )
			{
				Log_WarnF( "Client Tried Executing Command without flag to allow it: \"%s\"\n", commandName.c_str() );
				continue;
			}
		}

		Con_RunCommandArgs( commandName, args );
	}
}


void NetHelper_ReadVec2( const Vec2::Reader& srReader, glm::vec2& srVec )
{
	srVec.x = srReader.getX();
	srVec.y = srReader.getY();
}

void NetHelper_ReadVec3( const Vec3::Reader& srReader, glm::vec3& srVec )
{
	srVec.x = srReader.getX();
	srVec.y = srReader.getY();
	srVec.z = srReader.getZ();
}

void NetHelper_ReadVec4( const Vec4::Reader& srReader, glm::vec4& srVec )
{
	srVec.x = srReader.getX();
	srVec.y = srReader.getY();
	srVec.z = srReader.getZ();
	srVec.w = srReader.getW();
}


void NetHelper_WriteVec3( Vec2::Builder* spBuilder, const glm::vec2& srVec )
{
	spBuilder->setX( srVec.x );
	spBuilder->setY( srVec.y );
}

void NetHelper_WriteVec3( Vec3::Builder* spBuilder, const glm::vec3& srVec )
{
	spBuilder->setX( srVec.x );
	spBuilder->setY( srVec.y );
	spBuilder->setZ( srVec.z );
}

void NetHelper_WriteVec4( Vec4::Builder* spBuilder, const glm::vec4& srVec )
{
	spBuilder->setX( srVec.x );
	spBuilder->setY( srVec.y );
	spBuilder->setZ( srVec.z );
	spBuilder->setW( srVec.w );
}

