#pragma once

#include "graphics/imesh.h"
#include "graphics/igraphics.h"

#include "physics.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <unordered_map>
#include <vector>


class Skybox: public ISkyboxMesh
{
public:
	void                    Init();
	void                    SetSkybox( const std::string& path );
	void                    Draw();
	void                    SetAng( const glm::vec3& ang );

private:

	bool                    aValid = false;
};


Skybox& GetSkybox();

