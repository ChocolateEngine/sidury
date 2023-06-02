#pragma once

#include "network/net_main.h"
#include "capnproto/sidury.capnp.h"

struct Transform;
struct TransformSmall;
struct CCamera;
struct CPlayerMoveData;

constexpr int         CH_SERVER_PROTOCOL_VER = 1;


struct UserCmd_t
{
	glm::vec3 aAng;
	int       aButtons;
	bool      aNoclip;  // temp hack
};


// Are we hosting the server from our client?
bool                  Game_IsHosting();
bool                  Game_IsClient();
bool                  Game_IsServer();

// Entity Component stuff
Transform             NetComp_ReadTransform();
void                  NetComp_WriteTransform( capnp::MessageBuilder& srMessage, const Transform& srTransform );

TransformSmall        NetComp_ReadTransformSmall();
void                  NetComp_WriteTransformSmall( capnp::MessageBuilder& srMessage, const TransformSmall& srTransform );

CCamera               NetComp_ReadCamera();
void                  NetComp_WriteCamera( capnp::MessageBuilder& srMessage, const CCamera& srCamera );

// different, idk
void                  NetComp_UpdatePlayerMoveData( CPlayerMoveData& srMoveData );
void                  NetComp_WritePlayerMoveData( capnp::MessageBuilder& srMessage, const CPlayerMoveData& srMoveData );

