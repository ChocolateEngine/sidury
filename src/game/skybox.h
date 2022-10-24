#pragma once

#include "graphics/graphics.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <unordered_map>
#include <vector>



// NOTE: this should be just a single mesh class with one material
// and the vertex format shouldn't matter because we have this other stuff
class Skybox
{
public:
	void                             Init();
	void                             SetSkybox( const std::string& path );
	void                             Draw();
	void                             SetAng( const glm::vec3& ang );

	bool                             aValid = false;
	ModelDraw_t                      aDraw{};
	std::vector< float >             aMorphVerts;

	Skybox()  {};
	~Skybox()
	{
		if ( aDraw.aModel != InvalidHandle )
			Graphics_FreeModel( aDraw.aModel );
	}
};


Skybox& GetSkybox();

