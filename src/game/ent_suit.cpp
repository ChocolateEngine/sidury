#include "main.h"
#include "ent_suit.h"
#include "util.h"

SuitSystem* gSuitEntSystems[ 2 ] = { 0, 0 };

CONVAR( cl_suit_sound_offset_z, -4 );

SuitSystem::SuitSystem()
{
}

void SuitSystem::ComponentAdded( Entity sEntity, void* spData )
{
	CSuit* suit = ch_pointer_cast< CSuit >( spData ); // boolshit

	Log_Msg( "Suited up!\n" );
	//CH_ASSERT( aLogonSound ); later ^^
	if ( Game_ProcessingClient() )
	{
		suit->aLogonSound = audio->OpenSound( "sound/fvox/hev_logon.ogg" );

		if ( audio->IsValid( suit->aLogonSound ) )
		{
			glm::mat4 matrix;
			GetEntitySystem()->GetWorldMatrix( matrix, sEntity );

			glm::vec3 pos = Util_GetMatrixPosition( matrix );
			pos.z += cl_suit_sound_offset_z;

			audio->AddEffects( suit->aLogonSound, AudioEffect_World );
			audio->SetEffectData( suit->aLogonSound, EAudio_World_Pos, pos );
			audio->PlaySound( suit->aLogonSound );
		}
	}
}

void SuitSystem::Update()
{
	for ( Entity entity : aEntities )
	{
		CSuit* suit = Ent_GetComponent< CSuit >( entity, "suit" );  // boolshit

		if ( !suit )
			continue;

		if ( suit->aLogonSound )
		{
			if ( audio->IsValid( suit->aLogonSound ) )
			{
				glm::mat4 matrix;
				GetEntitySystem()->GetWorldMatrix( matrix, entity );

				glm::vec3 pos = Util_GetMatrixPosition( matrix );
				pos.z += cl_suit_sound_offset_z;

				audio->SetEffectData( suit->aLogonSound, EAudio_World_Pos, pos );
			}
			else
			{
				suit->aLogonSound = CH_INVALID_HANDLE;
			}
		}
	}
}

SuitSystem* GetSuitEntSys()
{
	int i = Game_ProcessingClient() ? 1 : 0;
	Assert( gSuitEntSystems[ i ] );
	return gSuitEntSystems[ i ];
}

CH_STRUCT_REGISTER_COMPONENT( CSuit, suit, EEntComponentNetType_Both, ECompRegFlag_None )
{
	CH_REGISTER_COMPONENT_SYS2( SuitSystem, gSuitEntSystems );
}

