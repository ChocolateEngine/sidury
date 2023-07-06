#pragma once


class ITool : public ISystem
{
  public:

	virtual void Enable()                   = 0;
	virtual void Disable()                  = 0;

	virtual void DrawUI()                   = 0;
};


bool Tools_Init();
void Tools_Shutdown();
void Tools_Update( float sFrameTime );
void Tools_DrawUI();

//void Tools_RegisterTool( ITool* spTool );

ITool* Tools_LoadToolModule( const char* spModule, const char* spInterface, size_t sVersion );

