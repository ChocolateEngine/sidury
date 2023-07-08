#include "system.h"
#include "tools.h"
#include "light_editor.h"
#include "entity_editor.h"
#include "map_converter.h"

#include "imgui/imgui.h"


// =========================================================================
// Higher level abstraction to wrap all tools in
// Maybe this would actually be benefit from an interface class
// =========================================================================


LOG_REGISTER_CHANNEL2( Tools, LogColor::Default );


CONVAR( tool_enabled, 1 );
CONVAR( tool_show_imgui_demo, 0 );


CONCMD( tool_show_toggle )
{
	tool_enabled = !tool_enabled;
}


static std::vector< ITool* > gTools;


bool Tools_Init()
{
	LightEditor_Init();

	if ( !EntEditor_Init() )
	{
		Log_Error( gLC_Tools, "Failed to init Entity Editor\n" );
		return false;
	}

	if ( !MapConverter_Init() )
	{
		Log_Error( gLC_Tools, "Failed to init Map Converter\n" );
		return false;
	}

	return true;
}


void Tools_Shutdown()
{
	LightEditor_Shutdown();
	EntEditor_Shutdown();
	MapConverter_Shutdown();
}


void Tools_Update( float sFrameTime )
{
	PROF_SCOPE();

	if ( !tool_enabled )
		return;

	LightEditor_Update();
	MapConverter_Update();
	EntEditor_Update( sFrameTime );
}


void Tools_DrawUI()
{
	PROF_SCOPE();

	if ( !tool_enabled )
		return;

	LightEditor_DrawUI();
	EntEditor_DrawUI();
	MapConverter_DrawUI();

	if ( tool_show_imgui_demo )
	{
		ImGui::ShowDemoWindow();
	}
}


ITool* Tools_LoadToolModule( const char* spModule, const char* spInterface, size_t sVersion )
{
	ITool*        tool = nullptr;
	AppModule_t   steamModule{ (ISystem**)&tool, spModule, spInterface, sVersion, false };
	EModLoadError ret = Mod_LoadAndInitSystem( steamModule );

	if ( ret != EModLoadError_Success )
	{
		Log_ErrorF( "Failed to Load Tool: %s\n", spInterface );
		return nullptr;
	}

	gTools.push_back( tool );
	return tool;

	// if ( !Mod_Load( spModule ) )
	// {
	// 	Log_ErrorF( gLC_Tools, "Failed to load module: %s\n", spModule );
	// 	return nullptr;
	// }
	// 
	// // add system we want from module
	// void* system = Mod_GetInterface( spInterface, sVersion );
	// if ( system == nullptr )
	// {
	// 	Log_ErrorF( gLC_Tools, "Failed to load system from module: %s - %s\n", spModule, spInterface );
	// 	return nullptr;
	// }
	// 
	// ITool* tool = static_cast< ITool* >( system );
	// 
	// if ( !tool->Init() )
	// {
	// 	Mod_Free( spModule );
	// 	return nullptr;
	// }

	//Mod_AddLoadedSystem( tool );
}

