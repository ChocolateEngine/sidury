#pragma once

#include "core/core.h"
#include "system.h"


class IToolkit;


enum EFileDialog
{
	EFileDialog_None,
	EFileDialog_Open,
	EFileDialog_Save,
};


struct ToolLaunchData
{
	IToolkit*   toolkit;
	SDL_Window* window;
	ChHandle_t  graphicsWindow;
	u32         mainViewport;
};


// Interface for Tools Interacting with the Main Toolkit
class IToolkit : public ISystem
{
	// Launch a Tool by name (maybe add args to start the tool with? like have the material editor open this material)
	// virtual void LaunchTool( const char* spTool )                                 = 0;

	// Focus a Tool Window
	// virtual void FocusTool( const char* spTool )                                  = 0;

	// Run a Command Line in a tool
	// Though, this is sort of useless with the concommand system you already have
	// just make sure every concommand is prefixed with tool name, like "mateditor_open", or "mapeditor_open"
	// virtual void RunToolArgs( const char* spTool, const char* spArgV, u32 sArgC ) = 0;

	// Open an Asset in a Tool
	// - spToolInterface is the name of the module interface
	// - spPath is the path to the asset to open
	virtual void OpenAsset( const char* spToolInterface, const char* spPath ) = 0;

	// virtual void DrawDialog( EFileDialog type )                                   = 0;
};


class ITool : public ISystem
{
   public:
	// Buttons
	// virtual ChHandle_t  GetIcon() = 0;

	virtual const char* GetName() = 0;

	virtual bool        Launch( const ToolLaunchData& launchData ) = 0;
	virtual void        Close()  = 0;

	// General Update
	virtual void        Update( float frameTime )  = 0;

	// Rendering Code + ImGui
	// sOffset is used when rendering in the main window as a tab
	virtual void        Render( float frameTime, glm::uvec2 sOffset )  = 0;

	// Tell this tool to try to open an asset for editing
	virtual bool        OpenAsset( const std::string& path )       = 0;
};


// all tool interfaces are currently stored here lmao, need to put this elsewhere
#define CH_TOOL_MAT_EDITOR_NAME "MaterialEditor"
#define CH_TOOL_MAT_EDITOR_VER  1

#define CH_TOOL_MAP_EDITOR_NAME "MapEditor"
#define CH_TOOL_MAP_EDITOR_VER  1

