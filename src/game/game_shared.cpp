#include "game_shared.h"
#include "entity.h"
#include "cl_main.h"
#include "sv_main.h"
#include "player.h"
#include "mapmanager.h"


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


// Convert a Client Source Message to String
const char* CL_MsgToString( EMsgSrc_Client sMsg )
{
	return EnumNameEMsgSrc_Client( sMsg );
}


// Convert a Server Source Message to String
const char* SV_MsgToString( EMsgSrc_Server sMsg )
{
	return EnumNameEMsgSrc_Server( sMsg );
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
	PROF_SCOPE();

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


void NetHelper_ReadVec2( const Vec2* spSource, glm::vec2& srVec )
{
	if ( !spSource )
		return;

	srVec.x = spSource->x();
	srVec.y = spSource->y();
}

void NetHelper_ReadVec3( const Vec3* spSource, glm::vec3& srVec )
{
	if ( !spSource )
		return;

	srVec.x = spSource->x();
	srVec.y = spSource->y();
	srVec.z = spSource->z();
}

void NetHelper_ReadVec4( const Vec4* spSource, glm::vec4& srVec )
{
	if ( !spSource )
		return;

	srVec.x = spSource->x();
	srVec.y = spSource->y();
	srVec.z = spSource->z();
	srVec.w = spSource->w();
}

void NetHelper_ReadQuat( const Quat* spSource, glm::quat& srQuat )
{
	if ( !spSource )
		return;

	srQuat.x = spSource->x();
	srQuat.y = spSource->y();
	srQuat.z = spSource->z();
	srQuat.w = spSource->w();
}

#if 0
void NetHelper_WriteVec3( Vec2Builder& srBuilder, const glm::vec2& srVec )
{
	srBuilder.add_x( srVec.x );
	srBuilder.add_y( srVec.y );
}

void NetHelper_WriteVec3( Vec3Builder& srBuilder, const glm::vec3& srVec )
{
	srBuilder.add_x( srVec.x );
	srBuilder.add_y( srVec.y );
	srBuilder.add_z( srVec.z );
}

void NetHelper_WriteVec4( Vec4Builder& srBuilder, const glm::vec4& srVec )
{
	srBuilder.add_x( srVec.x );
	srBuilder.add_y( srVec.y );
	srBuilder.add_z( srVec.z );
	srBuilder.add_z( srVec.w );
}
#endif

