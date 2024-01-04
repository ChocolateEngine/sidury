#include "inputsystem.h"
#include "iinput.h"
#include "main.h"
#include "game_shared.h"

extern IInputSystem* input;

LOG_REGISTER_CHANNEL2( GameInput, LogColor::Default );

NEW_CVAR_FLAG( CVARF_INPUT );

#define REGISTER_BUTTON( name ) ButtonInput_t name = Input_RegisterButton()

REGISTER_BUTTON( IN_FORWARD );
REGISTER_BUTTON( IN_BACK );


CONVAR( m_pitch, 0.022, CVARF_ARCHIVE );
CONVAR( m_yaw, 0.022, CVARF_ARCHIVE );
CONVAR( m_sensitivity, 1.0, CVARF_ARCHIVE, "Mouse Sensitivity" );


static glm::vec2                               gMouseDelta{};
static glm::vec2                               gMouseDeltaScale{ 1.f, 1.f };
static std::vector< ButtonInput_t >            gButtonInputs;
static ButtonInput_t                           gButtons;
static std::unordered_map< EButton, EBinding > gKeyBinds;
static std::unordered_map< EBinding, EButton > gBindingToKey;  // TODO: this only allows for one key/mouse button per action, i don't like that


static bool                                    gResetBindings = Args_Register( "Reset All Bindings", "-reset-binds" );


CONCMD_VA( in_dump_all_scancodes, "Dump a List of SDL2 Scancode strings" )
{
	LogGroup group = Log_GroupBegin( gLC_GameInput );

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


static void bind_dropdown_keys(
  const std::vector< std::string >& args,  // arguments currently typed in by the user
  std::vector< std::string >&       results )    // results to populate the dropdown list with
{
	for ( int i = 0; i < SDL_NUM_SCANCODES; i++ )
	{
		const char* name = SDL_GetScancodeName( (SDL_Scancode)i );

		if ( strlen( name ) == 0 )
			continue;

		if ( args.size() > 1 )
		{
			// Check if they are both equal
#ifdef _WIN32
			if ( _strnicmp( name, args[ 0 ].c_str(), strlen( name ) ) != 0 )
				continue;
#else
			if ( strncasecmp( name, args[ 0 ].c_str(), strlen( name ) ) != 0 )
				continue;
#endif
		}
		else if ( args.size() )
		{
			// Check if this string is inside this other string
			const char* find = strcasestr( name, args[ 0 ].c_str() );

			if ( !find )
				continue;
		}

		std::string& result = results.emplace_back();
		result += "\"";
		result += name;
		result += "\"";
	}
}

static void bind_dropdown(
  const std::vector< std::string >& args,  // arguments currently typed in by the user
  std::vector< std::string >&       results )    // results to populate the dropdown list with
{
	bind_dropdown_keys( args, results );

	if ( results.size() > 1 )
		return;

	// must be an invalid scancode
	if ( results.empty() )
	{
		results.push_back( "INVALID SCANCODE" );
		return;
	}

	std::string key = results[ 0 ] + " ";
	results.clear();

	// Build this spaced out command
	std::string command;
	for ( size_t i = 1; i < args.size(); i++ )
	{
		command += args[ i ];
		if ( i + 1 < args.size() )
			command += " ";
	}

	// Search Through ConVars
	std::vector< std::string > searchResults;
	Con_BuildAutoCompleteList( command, searchResults );

	for ( auto& cvar : searchResults )
	{
		results.push_back( key + cvar );
	}
}


static const char* gInputBindingStr[] = {

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


static void PrintBinding( EButton sScancode )
{
	// Find the command for this key and print it
	auto it = gKeyBinds.find( sScancode );
	if ( it == gKeyBinds.end() )
	{
		Log_MsgF( gLC_GameInput, "Binding: \"%s\" :\n", input->GetKeyName( sScancode ) );
	}
	else
	{
		Log_MsgF( "Binding: \"%s\" - %s\n", input->GetKeyName( sScancode ), Input_BindingToStr( it->second ) );
	}
}


CONVAR( in_show_scancodes, 0 );


void Input_Init()
{
	Input_CalcMouseDelta();

	// if ( gResetBindings )
	{
		Input_ResetBinds();
	}
}


void Input_Update()
{
	// update mouse inputs
	gMouseDelta = {};
	Input_CalcMouseDelta();
}


void Input_ResetBinds()
{
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


ButtonInput_t Input_RegisterButton()
{
	ButtonInput_t newBitShift = (1 << gButtonInputs.size());
	gButtonInputs.push_back( newBitShift );
	return newBitShift;
}


ButtonInput_t Input_GetButtonStates()
{
	return gButtons;
}


void Input_BindKey( SDL_Scancode sKey, EBinding sBinding )
{
	return Input_BindKey( (EButton)sKey, sBinding );
}


void Input_BindKey( EButton sKey, EBinding sBinding )
{
	if ( sBinding > EBinding_Count )
	{
		Log_ErrorF( gLC_GameInput, "Trying to bind key \"%s\" to Invalid Binding: \"%d\"\n", input->GetKeyName( sKey ), sBinding );
		return;
	}

	input->RegisterKey( sKey );

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

	Log_DevF( gLC_GameInput, 1, "Bound Key: \"%s\" \"%s\"\n", input->GetKeyName( sKey ), Input_BindingToStr( sBinding ) );
}


// IDEA: make an event system with keybindings?
// or, make a list of binding states, just a bunch of characters
// and input updates it per-frame
bool Input_KeyPressed( EBinding sKeyBind )
{
	auto it = gBindingToKey.find( sKeyBind );
	if ( it == gBindingToKey.end() )
		return false;

	return input->KeyPressed( it->second );
}


bool Input_KeyReleased( EBinding sKeyBind )
{
	auto it = gBindingToKey.find( sKeyBind );
	if ( it == gBindingToKey.end() )
		return false;

	return input->KeyReleased( it->second );
}


bool Input_KeyJustPressed( EBinding sKeyBind )
{
	auto it = gBindingToKey.find( sKeyBind );
	if ( it == gBindingToKey.end() )
		return false;

	return input->KeyJustPressed( it->second );
}


bool Input_KeyJustReleased( EBinding sKeyBind )
{
	auto it = gBindingToKey.find( sKeyBind );
	if ( it == gBindingToKey.end() )
		return false;

	return input->KeyJustReleased( it->second );
}

