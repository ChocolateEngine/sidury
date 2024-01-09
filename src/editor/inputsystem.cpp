#include "inputsystem.h"
#include "iinput.h"
#include "main.h"
#include "game_shared.h"

extern IInputSystem* input;

LOG_REGISTER_CHANNEL2( EditorInput, LogColor::Default );


CONVAR( m_pitch, 0.022, CVARF_ARCHIVE );
CONVAR( m_yaw, 0.022, CVARF_ARCHIVE );
CONVAR( m_sensitivity, 1.0, CVARF_ARCHIVE, "Mouse Sensitivity" );


struct ButtonList_t 
{
	EModMask aModMask;
	u8       aCount;
	EButton* apButtons;

	inline bool operator==( const ButtonList_t& srOther ) const
	{
		// Guard self assignment
		if ( this == &srOther )
			return true;

		if ( aModMask != srOther.aModMask )
			return false;

		if ( aCount != srOther.aCount )
			return false;

		return std::memcmp( &apButtons, &srOther.apButtons, sizeof( EButton* ) ) == 0;
	}
};


// Hashing Support
namespace std
{
	template<>
	struct hash< ButtonList_t >
	{
		size_t operator()( ButtonList_t const& list ) const
		{
			size_t value = 0;

			value ^= ( hash< EModMask >()( list.aModMask ) );
			value ^= ( hash< u8 >()( list.aCount ) );
			value ^= ( hash< EButton* >()( list.apButtons ) );

			return value;
		}
	};
}


static glm::vec2                                    gMouseDelta{};
static glm::vec2                                    gMouseDeltaScale{ 1.f, 1.f };
static std::unordered_map< ButtonList_t, EBinding > gKeyBinds;
static KeyState                                     gBindingState[ EBinding_Count ];

static bool                                         gResetBindings = Args_Register( "Reset All Bindings", "-reset-binds" );


// TODO: buffered input
// https://www.youtube.com/watch?v=VQ0Amoqz4Lg


// bind "ctrl+c" "command ent_copy"
// bind "ctrl+c" "move_forward"


CONCMD_VA( in_dump_all_scancodes, "Dump a List of SDL2 Scancode strings" )
{
	LogGroup group = Log_GroupBegin( gLC_EditorInput );

	Log_Group( group, "SDL2 Scancodes:\n\n" );
	for ( int i = 0; i < SDL_NUM_SCANCODES; i++ )
	{
		const char* name = SDL_GetScancodeName( (SDL_Scancode)i );

		if ( strlen( name ) == 0 )
			continue;

		Log_GroupF( group, "%d: %s\n", i, name );
	}

	Log_GroupEnd( group );
}


static const char* gInputBindingStr[] = {

	// General
	"General_Undo",
	"General_Redo",
	"General_Cut",
	"General_Copy",
	"General_Paste",

	// Viewport Bindings
	"Viewport_MouseLook",
	"Viewport_MoveForward",
	"Viewport_MoveBack",
	"Viewport_MoveLeft",
	"Viewport_MoveRight",
	"Viewport_MoveUp",
	"Viewport_MoveDown",
	"Viewport_Sprint",
	"Viewport_Slow",
	"Viewport_Select",
	"Viewport_IncreaseMoveSpeed",
	"Viewport_DecreaseMoveSpeed",
};


static_assert( CH_ARR_SIZE( gInputBindingStr ) == EBinding_Count );


const char* Input_BindingToStr( EBinding sBinding )
{
	if ( sBinding < 0 || sBinding > EBinding_Count )
		return "INVALID";

	return gInputBindingStr[ sBinding ];
}


//static void PrintBinding( EButton sScancode )
//{
//	// Find the command for this key and print it
//	auto it = gKeyBinds.find( sScancode );
//	if ( it == gKeyBinds.end() )
//	{
//		Log_MsgF( gLC_EditorInput, "Binding: \"%s\" :\n", input->GetKeyName( sScancode ) );
//	}
//	else
//	{
//		Log_MsgF( "Binding: \"%s\" - %s\n", input->GetKeyName( sScancode ), Input_BindingToStr( it->second ) );
//	}
//}


CONVAR( in_show_scancodes, 0 );


void Input_Init()
{
	Input_CalcMouseDelta();

	// if ( gResetBindings )
	{
		Input_ResetBinds();
	}
}


EModMask Input_CalcModMask()
{
	EModMask modMask = EModMask_None;

	if ( input->KeyPressed( (EButton)SDL_SCANCODE_LCTRL ) )
		modMask |= EModMask_CtrlL;

	if ( input->KeyPressed( (EButton)SDL_SCANCODE_RCTRL ) )
		modMask |= EModMask_CtrlR;

	if ( input->KeyPressed( (EButton)SDL_SCANCODE_LSHIFT ) )
		modMask |= EModMask_ShiftL;

	if ( input->KeyPressed( (EButton)SDL_SCANCODE_RSHIFT ) )
		modMask |= EModMask_ShiftR;

	if ( input->KeyPressed( (EButton)SDL_SCANCODE_LALT ) )
		modMask |= EModMask_AltL;

	if ( input->KeyPressed( (EButton)SDL_SCANCODE_RALT ) )
		modMask |= EModMask_AltR;

	if ( input->KeyPressed( (EButton)SDL_SCANCODE_LGUI ) )
		modMask |= EModMask_GuiL;

	if ( input->KeyPressed( (EButton)SDL_SCANCODE_RGUI ) )
		modMask |= EModMask_GuiR;

	return modMask;
}


EModMask Input_ConvertKeyToModMask( EButton spKey )
{
	switch ( spKey )
	{
		default:
			return EModMask_None;

		case SDL_SCANCODE_LCTRL:
			return EModMask_CtrlL;

		case SDL_SCANCODE_RCTRL:
			return EModMask_CtrlR;

		case SDL_SCANCODE_LSHIFT:
			return EModMask_ShiftL;

		case SDL_SCANCODE_RSHIFT:
			return EModMask_ShiftR;

		case SDL_SCANCODE_LALT:
			return EModMask_AltL;

		case SDL_SCANCODE_RALT:
			return EModMask_AltR;

		case SDL_SCANCODE_LGUI:
			return EModMask_GuiL;

		case SDL_SCANCODE_RGUI:
			return EModMask_GuiR;
	}

	return EModMask_None;
}


// Extract Modifier Keys from the Key List
EModMask Input_GetModMask( EButton* spKeys, u8 sKeyCount, ChVector< EButton >& srNewKeys )
{
	EModMask modMask = EModMask_None;

	for ( u8 i = 0; i < sKeyCount; i++ )
	{
		EModMask newMask = Input_ConvertKeyToModMask( spKeys[ i ] );

		if ( newMask )
		{
			modMask |= newMask;
		}
		else
		{
			srNewKeys.push_back( spKeys[ i ] );
		}
	}

	return modMask;
}


void Input_Update()
{
	// update mouse inputs
	gMouseDelta = {};
	Input_CalcMouseDelta();

	// Find current ModMask
	EModMask modMaskPressed = Input_CalcModMask();

	// Update Binding Key States
	for ( auto& [ key, value ] : gKeyBinds )
	{
		bool pressed = true;
		for ( u8 i = 0; i < key.aCount; i++ )
		{
			if ( !input->KeyPressed( key.apButtons[ i ] ) )
			{
				pressed = false;
				break;
			}
		}

		// If we have a key modifier mask on this key
		// make sure it matches the modifier mask currently being pressed
		if ( key.aModMask && key.aModMask != modMaskPressed )
			pressed = false;	

		KeyState state = gBindingState[ value ];

		// Is this currently pressed
		if ( state & KeyState_Pressed )
		{
			// Make sure we remove the just pressed state
			state &= ~KeyState_JustPressed;

			// If we released the key, remove both press types,
			// and add both release types (released and just released)
			if ( !pressed )
			{
				state |= KeyState_Released | KeyState_JustReleased;
				state &= ~( KeyState_Pressed | KeyState_JustPressed );
			}
		}
		// Is this key currently released?
		else if ( state & KeyState_Released )
		{
			// Make sure we remove the just released state
			state &= ~KeyState_JustReleased;

			// If we pressed the key, remove both release types,
			// and add both press types (pressed and just pressed)
			if ( pressed )
			{
				state |= KeyState_Pressed | KeyState_JustPressed;
				state &= ~( KeyState_Released | KeyState_JustReleased );
			}
		}
		else
		{
			// Mark this key as just released
			state |= KeyState_Released | KeyState_JustReleased;
			state &= ~( KeyState_Pressed | KeyState_JustPressed );
		}

		gBindingState[ value ] = state;
	}
}


void Input_ResetBinds()
{
	Input_BindKeys( { SDL_SCANCODE_LCTRL, SDL_SCANCODE_Z }, EBinding_General_Undo );
	Input_BindKeys( { SDL_SCANCODE_LCTRL, SDL_SCANCODE_Y }, EBinding_General_Redo );
	
	Input_BindKeys( { SDL_SCANCODE_LCTRL, SDL_SCANCODE_X }, EBinding_General_Cut );
	Input_BindKeys( { SDL_SCANCODE_LCTRL, SDL_SCANCODE_C }, EBinding_General_Copy );
	Input_BindKeys( { SDL_SCANCODE_LCTRL, SDL_SCANCODE_V }, EBinding_General_Paste );
	
	Input_BindKey( SDL_SCANCODE_SPACE, EBinding_Viewport_MouseLook );
	
	Input_BindKey( SDL_SCANCODE_W, EBinding_Viewport_MoveForward );
	Input_BindKey( SDL_SCANCODE_S, EBinding_Viewport_MoveBack );
	Input_BindKey( SDL_SCANCODE_A, EBinding_Viewport_MoveLeft );
	Input_BindKey( SDL_SCANCODE_D, EBinding_Viewport_MoveRight );
	Input_BindKey( SDL_SCANCODE_Q, EBinding_Viewport_MoveUp );
	Input_BindKey( SDL_SCANCODE_E, EBinding_Viewport_MoveDown );

	Input_BindKey( SDL_SCANCODE_LSHIFT, EBinding_Viewport_Sprint );
	Input_BindKey( SDL_SCANCODE_LCTRL, EBinding_Viewport_Slow );

	Input_BindKey( EButton_MouseLeft, EBinding_Viewport_Select );
}


void Input_CalcMouseDelta()
{
	const glm::ivec2& baseDelta = input->GetMouseDelta();

	gMouseDelta.x               = baseDelta.x * m_sensitivity;
	gMouseDelta.y               = baseDelta.y * m_sensitivity;
}


glm::vec2 Input_GetMouseDelta()
{
	return gMouseDelta * gMouseDeltaScale;
}


void Input_SetMouseDeltaScale( const glm::vec2& scale )
{
	gMouseDeltaScale = scale;
}


const glm::vec2& Input_GetMouseDeltaScale()
{
	return gMouseDeltaScale;
}


void Input_BindKey( EButton sKey, EBinding sBinding )
{
	Input_BindKeys( &sKey, 1, sBinding );

#if 0
	if ( sBinding > EBinding_Count )
	{
		Log_ErrorF( gLC_EditorInput, "Trying to bind key \"%s\" to Invalid Binding: \"%d\"\n", input->GetKeyName( sKey ), sBinding );
		return;
	}

	input->RegisterKey( sKey );

	bool foundKey = false;
	for ( auto& [ key, value ] : gKeyBinds )
	{
		if ( key.aCount != 1 )
			continue;

		if ( key.spButtons[ 0 ] == sKey )
		{
			foundKey = true;
		}

		for ( u8 i = 0; i < key.aCount; i++ )
	}

	auto it = gKeyBinds.find( sKey );
	if ( it == gKeyBinds.end() )
	{
		// bind it
		gKeyBinds[ sKey ]         = sBinding;
		gBindingToKey[ sBinding ] = sKey;
	}
	else
	{
		// update this bind (does this work?)
		it->second                = sBinding;
		gBindingToKey[ sBinding ] = sKey;
	}

	Log_DevF( gLC_EditorInput, 1, "Bound Key: \"%s\" \"%s\"\n", input->GetKeyName( sKey ), Input_BindingToStr( sBinding ) );
#endif
}


void Input_ClearBinding( EBinding sBinding )
{
	// auto it = gBindingToKey.find( sBinding );
	// if ( it == gBindingToKey.end() )
	// 	return;
	// 
	// gBindingToKey.erase( it );

	for ( auto& [ key, value ] : gKeyBinds )
	{
		if ( value != sBinding )
			continue;

		// Free Memory
		free( key.apButtons );

		gKeyBinds.erase( key );
		break;
	}
}


void Input_BindKey( SDL_Scancode sKey, EBinding sBinding )
{
	Input_BindKeys( (EButton*)&sKey, 1, sBinding );
	// Input_BindKey( (EButton)sKey, sBinding );
}


std::string Input_GetKeyComboName( EButton* spKeys, u8 sKeyCount )
{
	std::string keyList = input->GetKeyName( spKeys[ 0 ] );

	for ( u8 i = 1; i < sKeyCount; i++ )
	{
		keyList += "+";
		keyList += input->GetKeyName( spKeys[ i ] );
	}

	return keyList;
}


void Input_PrintNewBinding( EButton* spKeys, u8 sKeyCount, EBinding sBinding )
{
	Log_DevF( gLC_EditorInput, 1, "Bound Keys: \"%s\" - \"%s\"\n", Input_GetKeyComboName( spKeys, sKeyCount ).c_str(), Input_BindingToStr( sBinding ) );
}


void Input_BindKeys( EButton* spKeys, u8 sKeyCount, EBinding sBinding )
{
	if ( sKeyCount == 0 )
	{
		Log_Error( gLC_EditorInput, "Trying to bind 0 keys?" );
		return;
	}

	if ( sBinding > EBinding_Count )
	{
		Log_ErrorF( gLC_EditorInput, "Trying to bind key \"%s\" to Invalid Binding: \"%d\"\n", Input_GetKeyComboName( spKeys, sKeyCount ).c_str(), sBinding );
		return;
	}

	for ( u8 i = 0; i < sKeyCount; i++ )
		input->RegisterKey( spKeys[ i ] );

	ChVector< EButton > newKeyList;
	EModMask            modMask = Input_GetModMask( spKeys, sKeyCount, newKeyList );

	// this is kind weird
	for ( auto& [ key, value ] : gKeyBinds )
	{
		if ( newKeyList.size() != key.aCount )
			continue;

		if ( modMask != key.aModMask )
			continue;

		bool found = true;
		for ( u8 i = 0; i < key.aCount; i++ )
		{
			if ( spKeys[ i ] != key.apButtons[ i ] )
			{
				found = false;
				break;
			}
		}

		if ( found )
		{
			value = sBinding;

			Input_ClearBinding( sBinding );
			// gBindingToKey[ sBinding ] = key;

			Input_PrintNewBinding( spKeys, sKeyCount, sBinding );
			return;
		}
	}

	// Did not find key, allocate new one

	ButtonList_t buttonList{};
	buttonList.aModMask       = modMask;
	buttonList.aCount         = newKeyList.size();

	if ( buttonList.aCount )
		buttonList.apButtons = ch_malloc_count< EButton >( buttonList.aCount );

	for ( u8 i = 0; i < buttonList.aCount; i++ )
	{
		buttonList.apButtons[ i ] = newKeyList[ i ];
	}

	gKeyBinds[ buttonList ]   = sBinding;
	// gBindingToKey[ sBinding ] = buttonList;

	Input_PrintNewBinding( spKeys, sKeyCount, sBinding );
}


void Input_BindKeys( SDL_Scancode* spKeys, u8 sKeyCount, EBinding sBinding )
{
	return Input_BindKeys( (EButton*)spKeys, sKeyCount, sBinding );
}


void Input_BindKeys( std::vector< EButton > sKeys, EBinding sKeyBind )
{
	Input_BindKeys( sKeys.data(), sKeys.size(), sKeyBind );
}


void Input_BindKeys( std::vector< SDL_Scancode > sKeys, EBinding sKeyBind )
{
	Input_BindKeys( (EButton*)sKeys.data(), sKeys.size(), sKeyBind );
}


#if 1
bool Input_KeyPressed( EBinding sKeyBind )
{
	if ( sKeyBind > EBinding_Count )
		return false;

	return gBindingState[ sKeyBind ] & KeyState_Pressed;
}


bool Input_KeyReleased( EBinding sKeyBind )
{
	if ( sKeyBind > EBinding_Count )
		return false;

	return gBindingState[ sKeyBind ] & KeyState_Released;
}


bool Input_KeyJustPressed( EBinding sKeyBind )
{
	if ( sKeyBind > EBinding_Count )
		return false;

	return gBindingState[ sKeyBind ] & KeyState_JustPressed;
}


bool Input_KeyJustReleased( EBinding sKeyBind )
{
	if ( sKeyBind > EBinding_Count )
		return false;

	return gBindingState[ sKeyBind ] & KeyState_JustReleased;
}


KeyState Input_KeyState( EBinding sKeyBind )
{
	if ( sKeyBind > EBinding_Count )
		return KeyState_Invalid;

	return gBindingState[ sKeyBind ];
}

#else

bool Input_KeyPressed( EBinding sKeyBind )
{
	auto it = gBindingToKey.find( sKeyBind );
	if ( it == gBindingToKey.end() )
		return false;

	for ( u8 i = 0; i < it->second.aCount; i++ )
	{
		if ( !input->KeyPressed( it->second.spButtons[ i ] ) )
			return false;
	}

	return true;
}


bool Input_KeyReleased( EBinding sKeyBind )
{
	auto it = gBindingToKey.find( sKeyBind );
	if ( it == gBindingToKey.end() )
		return false;

	for ( u8 i = 0; i < it->second.aCount; i++ )
	{
		if ( !input->KeyReleased( it->second.spButtons[ i ] ) )
			return false;
	}

	return true;
}


bool Input_KeyJustPressed( EBinding sKeyBind )
{
	auto it = gBindingToKey.find( sKeyBind );
	if ( it == gBindingToKey.end() )
		return false;

	bool oneJustPressed = false;

	for ( u8 i = 0; i < it->second.aCount; i++ )
	{
		if ( !input->KeyPressed( it->second.spButtons[ i ] ) )
			return false;

		oneJustPressed |= input->KeyJustPressed( it->second.spButtons[ i ] );
	}

	return oneJustPressed;
}


bool Input_KeyJustReleased( EBinding sKeyBind )
{
	auto it = gBindingToKey.find( sKeyBind );
	if ( it == gBindingToKey.end() )
		return false;

	bool oneJustPressed = false;

	for ( u8 i = 0; i < it->second.aCount; i++ )
	{
		if ( !input->KeyReleased( it->second.spButtons[ i ] ) )
			return false;

		oneJustPressed |= input->KeyJustReleased( it->second.spButtons[ i ] );
	}

	return oneJustPressed;
}

#endif