#include "inputsystem.h"
#include "iinput.h"
#include "main.h"
#include "cl_main.h"
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


static glm::vec2                                       gMouseDelta{};
static glm::vec2                                       gMouseDeltaScale{ 1.f, 1.f };
static std::vector< ButtonInput_t >                    gButtonInputs;
static ButtonInput_t                                   gButtons;
static std::unordered_map< SDL_Scancode, std::string > gKeyBinds;
static std::unordered_map< SDL_Scancode, std::string > gKeyBindToggle;

static std::vector< ConVar* >                          gInputCvars;
static std::unordered_map< SDL_Scancode, ConVar* >     gInputCvarKeys;


static bool                                            gResetBindings = Args_Register( "Reset All Keybindings", "-reset-binds" );


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


static void PrintBinding( const char* spKey, const char* spCmd )
{
	Log_MsgF( gLC_GameInput, "Binding: \"%s\" \"%s\"\n", spKey, spCmd );
}


static void PrintBinding( SDL_Scancode sScancode, const char* spKey )
{
	// Find the command for this key and print it
	auto it = gKeyBinds.find( sScancode );
	if ( it == gKeyBinds.end() )
	{
		Log_MsgF( gLC_GameInput, "Binding: \"%s\" :\n", spKey );
	}
	else
	{
		PrintBinding( spKey, it->second.c_str() );
	}
}


CONCMD_DROP_VA( bind, bind_dropdown, 0, "Bind a key to a command" )
{
	if ( args.empty() )
		return;

	SDL_Scancode scancode = SDL_GetScancodeFromName( args[ 0 ].c_str() );

	if ( scancode == SDL_SCANCODE_UNKNOWN )
	{
		Log_ErrorF( gLC_GameInput, "Unknown Key: %s\n", args[ 0 ].c_str() );
		return;
	}

	if ( args.size() < 2 )
	{
		PrintBinding( scancode, args[ 0 ].c_str() );
		return;
	}

	if ( args.size() == 2 )
	{
		Input_BindKey( scancode, args[ 1 ] );
		return;
	}

	// Build this spaced out command
	std::string command;
	for ( size_t i = 1; i < args.size(); i++ )
	{
		command += args[ i ];
		if ( i + 1 < args.size() )
			command += " ";
	}

	Input_BindKey( scancode, command );
}


CONCMD_DROP_VA( unbind, bind_dropdown_keys, 0, "UnBind a key" )
{
	if ( args.empty() )
		return;

	SDL_Scancode scancode = SDL_GetScancodeFromName( args[ 0 ].c_str() );

	if ( scancode == SDL_SCANCODE_UNKNOWN )
	{
		Log_ErrorF( gLC_GameInput, "Unknown Key: \"%s\"\n", args[ 0 ].c_str() );
		return;
	}

	auto it = gKeyBinds.find( scancode );
	if ( it == gKeyBinds.end() )
	{
		Log_MsgF( gLC_GameInput, "Key is already unbound: \"%s\"\n", args[ 0 ].c_str() );
		return;
	}

	gInputCvarKeys.erase( scancode );
	gKeyBinds.erase( it );

	Log_DevF( gLC_GameInput, 1, "Unbound Key: \"%s\"\n", args[ 0 ].c_str() );
}


CONCMD_VA( bind_dump, "Dump all keys bound to a command" )
{
	for ( auto& [ scancode, command ] : gKeyBinds )
	{
		PrintBinding( SDL_GetScancodeName( scancode ), command.c_str() );
	}
}

CONCMD_VA( unbindall, "Unbind all keys" )
{
	// verify you want to do this
	if ( args.empty() || args[ 0 ] != "YES" )
	{
		Log_Msg( "If you really mean to unbind ALL your keys, type \"unbindall YES\"\n" );
		return;
	}

	gKeyBinds.clear();
	gKeyBindToggle.clear();
}


CONVAR( in_show_scancodes, 0 );


static const char* gDefaultBinds[] = {
	"bind \"W\" \"in_forward 1\"",
	"bind \"S\" \"in_back 1\"",
	"bind \"A\" \"in_left 1\"",
	"bind \"D\" \"in_right 1\"",
	"bind \"Space\" \"in_jump 1\"",
	"bind \"B\" \"fly\"",
	"bind \"V\" \"noclip\"",
	"bind \"Z\" \"in_zoom 1\"",
	"bind \"Left Ctrl\" \"in_duck 1\"",
	"bind \"Left Shift\" \"in_sprint 1\"",
	"bind \"F\" \"in_flashlight 1\"",
	"bind \"R\" \"create_proto\"",
	"bind \"T\" \"in_proto_spam 1\"",
	// "bind \"Escape\" \"pause\"",
};

constexpr size_t gDefaultBindSize = ARR_SIZE( gDefaultBinds );


CONCMD_VA( bind_reset_all, "Reset All Binds" )
{
	gKeyBinds.clear();
	gKeyBindToggle.clear();

	for ( int i = 0; i < gDefaultBindSize; i++ )
		Con_RunCommand( gDefaultBinds[ i ] );
}


static void CmdBindArchive( std::string& srOutput )
{
	srOutput += "// Bindings\n\n";

	for ( auto& [ scancode, command ] : gKeyBinds )
	{
		// get ugly'd on - Agent Agrimar
		srOutput += "bind \"";
		srOutput += SDL_GetScancodeName( scancode );
		srOutput += "\" \"";
		srOutput += command;
		srOutput += "\"\n";
	}
}


void Input_Init()
{
	Assert( Game_ProcessingClient() );

	Input_CalcMouseDelta();

	Con_AddArchiveCallback( CmdBindArchive );

	// Find all Convars with the CVARF_INPUT flag on it
	for ( uint32_t i = 0; i < Con_GetConVarCount(); i++ )
	{
		ConVarBase* current = Con_GetConVar( i );

		if ( typeid( *current ) != typeid( ConVar ) )
			continue;

		ConVar* cvar = static_cast< ConVar* >( current );

		if ( cvar->aFlags & CVARF_INPUT )
			gInputCvars.push_back( cvar );
	}

	if ( gResetBindings )
	{
		bind_reset_all( {} );
	}
}


void Input_Update()
{
	Assert( Game_ProcessingClient() );

	// update mouse inputs
	gMouseDelta = {};
	Input_CalcMouseDelta();

	// Don't run inputs when the menu is shown
	if ( !CL_IsMenuShown() )
	{
		// Update button binds and run the commands they are bound to
		for ( auto& [ scancode, command ] : gKeyBinds )
		{
			if ( input->KeyJustPressed( scancode ) )
				Con_RunCommand( command );
		}
	}

	// Update Input ConVar States
	for ( auto& [ scancode, cvar ] : gInputCvarKeys )
	{
		if ( input->KeyJustPressed( scancode ) )
		{
			cvar->SetValue( IN_CVAR_JUST_PRESSED );
		}
		else if ( input->KeyPressed( scancode ) && cvar->aValueFloat == IN_CVAR_JUST_PRESSED )
		{
			cvar->SetValue( IN_CVAR_PRESSED );
		}
		else if ( input->KeyJustReleased( scancode ) )
		{
			cvar->SetValue( IN_CVAR_JUST_RELEASED );
		}
		else if ( input->KeyReleased( scancode ) && cvar->aValueFloat == IN_CVAR_JUST_RELEASED )
		{
			cvar->SetValue( IN_CVAR_RELEASED );
		}
	}
	
	// and for updating button states, do something like this
	// gButtons = 0;

	// if ( in_forward == 1.f )
	// 	gButtons |= IN_FORWARD;
	// 
	// else if ( in_forward == -1.f )
	// 	gButtons |= IN_BACK;
}


void Input_CalcMouseDelta()
{
	Assert( Game_ProcessingClient() );

	const glm::ivec2& baseDelta = input->GetMouseDelta();

	gMouseDelta.x = baseDelta.x * m_sensitivity;
	gMouseDelta.y = baseDelta.y * m_sensitivity;
}


glm::vec2 Input_GetMouseDelta()
{
	Assert( Game_ProcessingClient() );

	return gMouseDelta * gMouseDeltaScale;
}


void Input_SetMouseDeltaScale( const glm::vec2& scale )
{
	Assert( Game_ProcessingClient() );

	gMouseDeltaScale = scale;
}


const glm::vec2& Input_GetMouseDeltaScale()
{
	Assert( Game_ProcessingClient() );

	return gMouseDeltaScale;
}


ButtonInput_t Input_RegisterButton()
{
	Assert( Game_ProcessingClient() );

	ButtonInput_t newBitShift = (1 << gButtonInputs.size());
	gButtonInputs.push_back( newBitShift );
	return newBitShift;
}


ButtonInput_t Input_GetButtonStates()
{
	Assert( Game_ProcessingClient() );

	return gButtons;
}


void Input_BindKey( SDL_Scancode key, const std::string& cmd )
{
	Assert( Game_ProcessingClient() );

	input->RegisterKey( key );

	auto it = gKeyBinds.find( key );
	if ( it == gKeyBinds.end() )
	{
		// bind it
		gKeyBinds[key] = cmd;
	}
	else
	{
		// update this bind (does this work?)
		it->second = cmd;
	}

	Log_DevF( gLC_GameInput, 1, "Bound Key: \"%s\" \"%s\"\n", SDL_GetScancodeName( key ), cmd.c_str() );

	// Add the first convar here to the input list
	std::string name;
	std::vector< std::string > args;
	Con_ParseCommandLine( cmd, name, args );

	ConVar* cvar = Con_GetConVar( name );

	if ( !cvar )
		return;

	if ( cvar->aFlags & CVARF_INPUT )
	{
		gInputCvarKeys[ key ] = cvar;
	}
}

