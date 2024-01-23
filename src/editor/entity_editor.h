#pragma once

#include "render/irender.h"


// Entity Editor
bool       EntEditor_Init();
void       EntEditor_Shutdown();
void       EntEditor_Update( float sFrameTime );
void       EntEditor_DrawUI();

void       Editor_DrawTextureInfo( TextureInfo_t& info );

// Asset Browser
void       AssetBrowser_Draw();

// Material Editor
void       MaterialEditor_Draw();
void       MaterialEditor_SetMaterial( ChHandle_t sMat );
ChHandle_t MaterialEditor_GetMaterial();

