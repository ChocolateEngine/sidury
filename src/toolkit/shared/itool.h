#pragma once

#include "core/core.h"
#include "system.h"


// Interface for Tools Interacting with the Main Toolkit
class IToolkit : public ISystem
{
	// Launch a Tool by name (maybe add args to start the tool with? like have the material editor open this material)
	virtual void LaunchTool( const char* spTool )                                 = 0;

	// Focus a Tool Window
	virtual void FocusTool( const char* spTool )                                  = 0;

	// Run a Command Line in a tool
	// Though, this is sort of useless with the concommand system you already have
	// just make sure every concommand is prefixed with tool name, like "mateditor_open", or "mapeditor_open"
	virtual void RunToolArgs( const char* spTool, const char* spArgV, u32 sArgC ) = 0;
};


class ITool : public ISystem
{
	// Buttons
	// virtual ChHandle_t  GetIcon() = 0;

	virtual const char* GetName() = 0;

	virtual bool        Launch( IToolkit* spToolkit )  = 0;
	virtual void        Close()  = 0;
};


