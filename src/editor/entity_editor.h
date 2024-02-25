#pragma once

#include "render/irender.h"

#include "main.h"
#include "entity.h"


struct EditorRenderables
{
	ChHandle_t          gizmoTranslation;
	ChHandle_t          gizmoRotation;
	ChHandle_t          gizmoScale;

	// Base AABB's for translation axis
	AABB                baseTranslateAABB[ 3 ];
};

extern EditorRenderables gEditorRenderables;


// Entity Editor
bool                     EntEditor_Init();
void                     EntEditor_Shutdown();
void                     EntEditor_Update( float sFrameTime );
void                     EntEditor_DrawUI();

void                     EntEditor_LoadEditorRenderable( ChVector< const char* >& failList, ChHandle_t& handle, const char* path );

void                     Editor_DrawTextureInfo( TextureInfo_t& info );

// adds the entity to the selection list, making sure it's not in the list multiple times
void                     EntEditor_AddToSelection( EditorContext_t* context, ChHandle_t entity );

// Asset Browser
void                     AssetBrowser_Draw();

// Material Editor
void                     MaterialEditor_Draw();
void                     MaterialEditor_SetMaterial( ChHandle_t sMat );
ChHandle_t               MaterialEditor_GetMaterial();
