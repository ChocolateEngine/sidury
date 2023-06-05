#include "game_shared.h"
#include "cl_main.h"
#include "sv_main.h"
#include "entity.h"
#include "player.h"

#include <capnp/message.h>
#include <capnp/serialize-packed.h>


NEW_CVAR_FLAG( CVARF_CL_EXEC );
NEW_CVAR_FLAG( CVARF_SV_EXEC );


static bool           gGameUseClient     = true;
static ECommandSource gGameCommandSource = ECommandSource_Client;


CONCMD( status )
{
	size_t playerCount = GetPlayers()->aPlayerList.size();

	// if ( Game_IsHosting() )
	// {
	// 	playerCount = gServerData.aClients.size();
	// }
	// else
	// {
	// 	playerCount = gClientServerData.aPlayerCount;
	// }

	Log_MsgF( "%zd Players Currently on Server\n", playerCount );
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

		ConVarFlag_t flags = cvarBase->GetFlags();

		// if the command is from the server and we are the client, make sure they can execute it
		if ( sSource == ECommandSource_Server && Game_ProcessingClient() )
		{
			// The Convar must have this flag
			if ( !(flags & CVARF_SV_EXEC) )
			{
				Log_WarnF( "Server Tried Executing Command without flag to allow it: \"%s\"\n", commandName );
				continue;
			}
		}

		// if the command is from the client and we are the server, make sure they can execute it
		else if ( sSource == ECommandSource_Client && !Game_ProcessingClient() )
		{
			// The Convar must have this flag
			if ( !(flags & CVARF_CL_EXEC) )
			{
				Log_WarnF( "Client Tried Executing Command without flag to allow it: \"%s\"\n", commandName );
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

