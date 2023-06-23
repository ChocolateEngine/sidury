#include "tools.h"
#include "light_editor.h"
#include "entity_editor.h"

#include "imgui/imgui.h"

// =========================================================================
// Higher level abstraction to wrap all tools in
// Maybe this would actually be benefit from an interface class
// =========================================================================


CONVAR( tool_enabled, 1 );
CONVAR( tool_show_imgui_demo, 0 );


bool Tools_Init()
{
	LightEditor_Init();

	if ( !EntEditor_Init() )
	{
		Log_Error( "Failed to init Entity Editor\n" );
		return false;
	}

	return true;
}


void Tools_Shutdown()
{
	LightEditor_Shutdown();
	EntEditor_Shutdown();
}


void Tools_Update( float sFrameTime )
{
	if ( !tool_enabled )
		return;

	LightEditor_Update();
	EntEditor_Update( sFrameTime );
}


void Tools_DrawUI()
{
	if ( !tool_enabled )
		return;

	LightEditor_DrawUI();
	EntEditor_DrawUI();

	if ( tool_show_imgui_demo )
	{
		ImGui::ShowDemoWindow();
	}
}

