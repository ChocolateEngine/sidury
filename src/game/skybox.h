#pragma once

#include "graphics/igraphics.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <unordered_map>
#include <vector>



// NOTE: this should be just a single mesh class with one material
// and the vertex format shouldn't matter because we have this other stuff
class Skybox : public Model
{
public:
	void                             Init();
	void                             SetSkybox( const std::string& path );
	void                             Draw();
	void                             SetAng( const glm::vec3& ang );

#if 0
	IMaterial*                       apMaterial = nullptr;

	std::vector< vertex_cube_3d_t >  aVertices;
	std::vector< uint32_t >          aIndices;

	// --------------------------------------------------------------------------------------
	// Materials

	virtual size_t     GetMaterialCount() override                          { return 1; }
	virtual IMaterial* GetMaterial( size_t i = 0 ) override                 { return apMaterial; }
	virtual void       SetMaterial( size_t i, IMaterial* mat ) override     { apMaterial = mat; }

	virtual void SetShader( size_t i, const std::string& name )
	{
		if ( apMaterial )
			apMaterial->SetShader( name );
	}

	// --------------------------------------------------------------------------------------
	// Drawing Info

	virtual u32 GetVertexOffset( size_t material ) override     { return 0; }
	virtual u32 GetVertexCount( size_t material ) override      { return aVertices.size(); }

	virtual u32 GetIndexOffset( size_t material ) override      { return 0; }
	virtual u32 GetIndexCount( size_t material ) override       { return aIndices.size(); }

	// --------------------------------------------------------------------------------------
	// Part of IModel only, i still don't know how i would handle different vertex formats
	// maybe store it in some kind of unordered_map containing these models and the vertex type?
	// but, the vertex and index type needs to be determined by the shader pipeline actually, hmm

	virtual VertexFormatFlags                   GetVertexFormatFlags() override     { return g_vertex_flags_cube; }
	virtual size_t                              GetVertexFormatSize() override      { return sizeof( vertex_cube_3d_t ); }
	virtual void*                               GetVertexData() override            { return aVertices.data(); };
	virtual size_t                              GetTotalVertexCount() override      { return aVertices.size(); };

	virtual std::vector< vertex_cube_3d_t >&    GetVertices()                       { return aVertices; };
	virtual std::vector< uint32_t >&            GetIndices() override               { return aIndices; };
#endif

	bool                    aValid = false;

	Skybox()  {};
	~Skybox() {}
};


Skybox& GetSkybox();

