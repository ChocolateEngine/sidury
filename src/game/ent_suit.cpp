#include "main.h"
#include "ent_suit.h"
#include "util.h"

SuitSystem* gSuitEntSystems[ 2 ] = { 0, 0 };

SuitSystem::SuitSystem()
{
	aLogonSound = CH_INVALID_HANDLE;  //audio->LoadSound( "sound/fvox/hev_logon.ogg" ); later
}

void SuitSystem::ComponentAdded( Entity sEntity, void* spData )
{
	CSuit* aSuit = ch_pointer_cast< CSuit >( spData ); // boolshit

	Log_Msg( "Suited up!\n" );
	//CH_ASSERT( aLogonSound ); later ^^
	if ( Game_ProcessingClient() )
	{
		aLogonSound = audio->OpenSound( "sound/fvox/hev_logon.ogg" );
		audio->PlaySound( aLogonSound );
	}
}

SuitSystem* GetSuitEntSys()
{
	int i = Game_ProcessingClient() ? 1 : 0;
	Assert( gSuitEntSystems[ i ] );
	return gSuitEntSystems[ i ];
}

CH_STRUCT_REGISTER_COMPONENT( CSuit, suit, true, EEntComponentNetType_Both, CH_ENT_SAVE_TO_MAP )
{
	CH_REGISTER_COMPONENT_SYS2( SuitSystem, gSuitEntSystems );
}

