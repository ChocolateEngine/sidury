#pragma once

#include "network/net_main.h"
#include "capnproto/sidury.capnp.h"

struct Transform;
struct TransformSmall;
struct CCamera;
struct CPlayerMoveData;
enum class PlayerMoveType;

constexpr int CH_SERVER_PROTOCOL_VER = 1;

EXT_CVAR_FLAG( CVARF_CL_EXEC );
EXT_CVAR_FLAG( CVARF_SV_EXEC );


enum ECommandSource
{
	// Console Command Came from the client
	ECommandSource_Client,

	// Console Command Came from the server
	ECommandSource_Server,
};


// Keep in sync with NetMsgUserCmd in sidury.capnp
struct UserCmd_t
{
	glm::vec3      aAng;
	int            aButtons;
	PlayerMoveType aMoveType;
	bool           aFlashlight;  // Temp, toggles the flashlight on/off
};


// Are we hosting the server from our client?
bool                  Game_IsHosting();
bool                  Game_IsClient();
bool                  Game_IsServer();

// Should we use client or server versions of systems?
bool                  Game_ProcessingClient();
bool                  Game_ProcessingServer();
void                  Game_SetClient( bool client = true );

ECommandSource        Game_GetCommandSource();
void                  Game_SetCommandSource( ECommandSource sSource );
void                  Game_ExecCommandsSafe( ECommandSource sSource, std::string_view sCommand );

// Network Helper functions
void                  NetHelper_ReadVec3( const Vec3::Reader& srReader, glm::vec3& srVec3 );
void                  NetHelper_WriteVec3( Vec3::Builder* spBuilder, const glm::vec3& srVec3 );

