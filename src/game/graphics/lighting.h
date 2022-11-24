#pragma once

bool   Graphics_CreateLightDescriptorSets();

void   Graphics_DestroyShadowRenderPass();
bool   Graphics_CreateShadowRenderPass();

void   Graphics_AddShadowMap( Light_t* spLight );
void   Graphics_DestroyShadowMap( Light_t* spLight );

Handle Graphics_AddLightBuffer( const char* spBufferName, size_t sBufferSize, Light_t* spLight );
void   Graphics_DestroyLightBuffer( Light_t* spLight );
void   Graphics_UpdateLightBuffer( Light_t* spLight );

void   Graphics_PrepareLights();

// Are we using any shadowmaps/are any shadowmaps enabled?
bool   Graphics_IsUsingShadowMaps();

void   Graphics_DrawShadowMaps( Handle cmd );

