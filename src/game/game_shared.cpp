#include "game_shared.h"
#include "sv_main.h"
#include "entity.h"
#include "player.h"

#include <capnp/message.h>
#include <capnp/serialize-packed.h>


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

// Entity Component stuff
Transform NetComp_ReadTransform()
{
	return {};
}

void NetComp_WriteTransform( capnp::MessageBuilder& srMessage, const Transform& srTransform )
{
	auto          builder = srMessage.initRoot< NetCompTransform >();

	Vec3::Builder pos = builder.getPos();
	pos.setX( srTransform.aPos.x );
	pos.setY( srTransform.aPos.y );
	pos.setZ( srTransform.aPos.z );

	Vec3::Builder ang = builder.getAng();
	ang.setX( srTransform.aAng.x );
	ang.setY( srTransform.aAng.y );
	ang.setZ( srTransform.aAng.z );

	Vec3::Builder scale = builder.getScale();
	scale.setX( srTransform.aScale.x );
	scale.setY( srTransform.aScale.y );
	scale.setZ( srTransform.aScale.z );
}

TransformSmall NetComp_ReadTransformSmall()
{
	return {};
}

void NetComp_WriteTransformSmall( capnp::MessageBuilder& srMessage, const TransformSmall& srTransform )
{
	auto          builder = srMessage.initRoot< NetCompTransform >();

	Vec3::Builder pos = builder.getPos();
	pos.setX( srTransform.aPos.x );
	pos.setY( srTransform.aPos.y );
	pos.setZ( srTransform.aPos.z );

	Vec3::Builder ang = builder.getAng();
	ang.setX( srTransform.aAng.x );
	ang.setY( srTransform.aAng.y );
	ang.setZ( srTransform.aAng.z );
}

CCamera NetComp_ReadCamera()
{
	return {};
}

void NetComp_WriteCamera( capnp::MessageBuilder& srMessage, const CCamera& srCamera )
{
	auto builder = srMessage.initRoot< NetCompCamera >();

	builder.setFov( srCamera.aFov );

	Vec3::Builder pos = builder.getTransform().getPos();
	pos.setX( srCamera.aTransform.aPos.x );
	pos.setY( srCamera.aTransform.aPos.y );
	pos.setZ( srCamera.aTransform.aPos.z );

	Vec3::Builder ang = builder.getTransform().getAng();
	ang.setX( srCamera.aTransform.aAng.x );
	ang.setY( srCamera.aTransform.aAng.y );
	ang.setZ( srCamera.aTransform.aAng.z );
}

// different, idk
void NetComp_UpdatePlayerMoveData( CPlayerMoveData& srMoveData )
{
}

void NetComp_WritePlayerMoveData( capnp::MessageBuilder& srMessage, const CPlayerMoveData& srMoveData )
{
	auto builder = srMessage.initRoot< NetCompPlayerMoveData >();

	switch ( srMoveData.aMoveType )
	{
		case PlayerMoveType::Walk:
			builder.setMoveType( EPlayerMoveType::WALK );

		default:
		case PlayerMoveType::NoClip:
			builder.setMoveType( EPlayerMoveType::NO_CLIP );

		case PlayerMoveType::Fly:
			builder.setMoveType( EPlayerMoveType::FLY );
	}

	builder.setPlayerFlags( srMoveData.aPlayerFlags );
	builder.setPrevPlayerFlags( srMoveData.aPrevPlayerFlags );
	builder.setMaxSpeed( srMoveData.aMaxSpeed );

	// Smooth Duck
	builder.setPrevViewHeight( srMoveData.aPrevViewHeight );
	builder.setTargetViewHeight( srMoveData.aTargetViewHeight );
	builder.setOutViewHeight( srMoveData.aOutViewHeight );
	builder.setDuckDuration( srMoveData.aDuckDuration );
	builder.setDuckTime( srMoveData.aDuckTime );
}

