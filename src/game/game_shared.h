#pragma once

#include "network/net_main.h"
#include "capnproto/sidury.capnp.h"

struct Transform;
struct TransformSmall;
struct CCamera;
struct CPlayerMoveData;
enum class PlayerMoveType;


EXT_CVAR_FLAG( CVARF_CL_EXEC );
EXT_CVAR_FLAG( CVARF_SV_EXEC );


enum ECommandSource
{
	// Console Command was sent to us from the Client
	ECommandSource_Client,

	// Console Command was sent to us from the Server
	ECommandSource_Server,

	// Console Command came from the Console
	ECommandSource_Console,
};


// Keep in sync with NetMsgUserCmd in sidury.capnp
struct UserCmd_t
{
	glm::vec3      aAng;
	int            aButtons;
	PlayerMoveType aMoveType;
	bool           aFlashlight;  // Temp, toggles the flashlight on/off
};


// Utility Functions

// Convert a Client Source Message to String
const char*           CL_MsgToString( EMsgSrcClient sMsg );

// Convert a Server Source Message to String
const char*           SV_MsgToString( EMsgSrcServer sMsg );


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
void                  NetHelper_ReadVec2( const Vec2::Reader& srReader, glm::vec2& srVec );
void                  NetHelper_ReadVec3( const Vec3::Reader& srReader, glm::vec3& srVec );
void                  NetHelper_ReadVec4( const Vec4::Reader& srReader, glm::vec4& srVec );

void                  NetHelper_WriteVec2( Vec2::Builder* spBuilder, const glm::vec2& srVec );
void                  NetHelper_WriteVec3( Vec3::Builder* spBuilder, const glm::vec3& srVec );
void                  NetHelper_WriteVec4( Vec4::Builder* spBuilder, const glm::vec4& srVec );

