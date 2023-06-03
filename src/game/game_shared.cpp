#include "game_shared.h"
#include "sv_main.h"
#include "entity.h"
#include "player.h"

#include <capnp/message.h>
#include <capnp/serialize-packed.h>


NEW_CVAR_FLAG( CVARF_CL_EXEC );
NEW_CVAR_FLAG( CVARF_SV_EXEC );


static bool           gGameUseClient     = true;
static ECommandSource gGameCommandSource = ECommandSource_Client;

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


void NetHelper_ReadVec3( const Vec3::Reader& srReader, glm::vec3& srVec3 )
{
	srVec3.x = srReader.getX();
	srVec3.y = srReader.getY();
	srVec3.z = srReader.getZ();
}

void NetHelper_WriteVec3( Vec3::Builder* spBuilder, const glm::vec3& srVec3 )
{
	spBuilder->setX( srVec3.x );
	spBuilder->setY( srVec3.y );
	spBuilder->setZ( srVec3.z );
}

