/*
modelloader.cpp ( Authored by Demez )

Class dedicated for loading models, and caches them too for multiple uses
*/

#include "util.h"
#include "core/console.h"
#include "render/irender.h"

#include "graphics.h"
#include "mesh_builder.h"

#include <chrono>

extern IRender* render;

#define FAST_OBJ_IMPLEMENTATION
#include "fast_obj/fast_obj.h"

static std::string GetBaseDir( const std::string &srPath )
{
	if ( srPath.find_last_of( "/\\" ) != std::string::npos )
		return srPath.substr( 0, srPath.find_last_of( "/\\" ) );
	return "";
}


static std::string gDefaultShader = "basic_3d";

static std::string MatVar_Diffuse = "diffuse";
static std::string MatVar_Emissive = "emissive";


void LoadObj_Fast( const std::string &srBasePath, const std::string &srPath, Model* spModel )
{
	fastObjMesh* obj = fast_obj_read( srPath.c_str() );

	if ( obj == nullptr )
	{
		Log_ErrorF( gLC_ClientGraphics, "Failed to parse obj\n", srPath.c_str() );
		return;
	}

	std::string baseDir  = GetBaseDir( srPath );
	std::string baseDir2 = GetBaseDir( srBasePath );

	MeshBuilder meshBuilder;
  #ifdef _DEBUG
	meshBuilder.Start( spModel, srPath.c_str() );
  #else
	meshBuilder.Start( spModel );
  #endif
	meshBuilder.SetSurfaceCount( obj->material_count );
	
	for ( unsigned int i = 0; i < obj->material_count; i++ )
	{
		fastObjMaterial& objMat   = obj->materials[ i ];
		std::string      matName  = baseDir2 + "/" + objMat.name;
		Handle           material = Graphics_FindMaterial( matName.c_str() );

		if ( material == InvalidHandle )
		{
			std::string matPath = matName + ".cmt";
			if ( FileSys_IsFile( matPath ) )
				material = Graphics_LoadMaterial( matPath );
		}

		// fallback if there is no cmt file
		if ( material == InvalidHandle )
		{
			material = Graphics_CreateMaterial( objMat.name, Graphics_GetShader( gDefaultShader ) );

			TextureCreateData_t createData{};
			createData.aUsage  = EImageUsage_Sampled;
			createData.aFilter = EImageFilter_Linear;

			auto SetTexture    = [ & ]( const std::string& param, const char* texname )
			{
				if ( texname == nullptr )
					return;

				Handle texture = InvalidHandle;

				if ( FileSys_IsRelative( texname ) )
					render->LoadTexture( texture, baseDir2 + "/" + texname, createData );
				else
					render->LoadTexture( texture, texname, createData );

				Mat_SetVar( material, param, texture );
			};

			SetTexture( MatVar_Diffuse, objMat.map_Kd.path );
			SetTexture( MatVar_Emissive, objMat.map_Ke.path );
		}

		meshBuilder.SetCurrentSurface( i );
		meshBuilder.SetMaterial( material );
	}

	// u64 vertexOffset = 0;
	// u64 indexOffset  = 0;
	// 
	// u64 totalVerts = 0;
	u64 totalIndexOffset  = 0;

	for ( u32 objIndex = 0; objIndex < obj->object_count; objIndex++ )
	// for ( u32 objIndex = 0; objIndex < obj->group_count; objIndex++ )
	{
		fastObjGroup& group = obj->objects[objIndex];
		// fastObjGroup& group = obj->groups[objIndex];

		// index_offset += group.index_offset;

		for ( u32 faceIndex = 0; faceIndex < group.face_count; faceIndex++ )
		// for ( u32 faceIndex = 0; faceIndex < obj->face_count; faceIndex++ )
		{
			u32& faceVertCount = obj->face_vertices[group.face_offset + faceIndex];
			u32& faceMat = obj->face_materials[group.face_offset + faceIndex];

			meshBuilder.SetCurrentSurface( faceMat );

			for ( u32 faceVertIndex = 0; faceVertIndex < faceVertCount; faceVertIndex++ )
			{
				// NOTE: mesh->indices holds each face "fastObjIndex" as three
				// seperate index objects contiguously laid out one after the other
				// fastObjIndex objIndex = obj->indices[totalIndexOffset + faceVertIndex];
				fastObjIndex objIndex = obj->indices[totalIndexOffset + faceVertIndex];

				if ( faceVertIndex >= 3 )
				{
					auto ind0 = meshBuilder.apSurf->aIndices[ meshBuilder.apSurf->aIndices.size() - 3 ];
					auto ind1 = meshBuilder.apSurf->aIndices[ meshBuilder.apSurf->aIndices.size() - 1 ];

					meshBuilder.apSurf->aVertex = meshBuilder.apSurf->aVertices[ ind0 ];
					meshBuilder.NextVertex();

					meshBuilder.apSurf->aVertex = meshBuilder.apSurf->aVertices[ ind1 ];
					meshBuilder.NextVertex();
				}

				const u32 position_index = objIndex.p * 3;
				const u32 texcoord_index = objIndex.t * 2;
				const u32 normal_index   = objIndex.n * 3;

				meshBuilder.SetPos(
				  obj->positions[ position_index ],
				  obj->positions[ position_index + 1 ],
				  obj->positions[ position_index + 2 ] );

				meshBuilder.SetNormal(
				  obj->normals[ normal_index ],
				  obj->normals[ normal_index + 1 ],
				  obj->normals[ normal_index + 2 ] );

				meshBuilder.SetTexCoord(
				  obj->texcoords[ texcoord_index ],
				  1.0f - obj->texcoords[ texcoord_index + 1 ] );

				meshBuilder.NextVertex();
			}

			// indexOffset += faceVertCount;
			totalIndexOffset += faceVertCount;
		}
	}

	meshBuilder.End();
	fast_obj_destroy( obj );
}


void Graphics_LoadObj( const std::string& srBasePath, const std::string& srPath, Model* spModel )
{
	auto startTime = std::chrono::high_resolution_clock::now();
	float time      = 0.f;

	LoadObj_Fast( srBasePath, srPath, spModel );

	auto currentTime = std::chrono::high_resolution_clock::now();
	time = std::chrono::duration< float, std::chrono::seconds::period >( currentTime - startTime ).count();

	Log_DevF( gLC_ClientGraphics, 1, "Obj Load Time: %.6f sec: %s\n", time, srBasePath.c_str() );
}

