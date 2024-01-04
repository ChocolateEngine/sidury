#pragma once

#include <glm/vec3.hpp>

#include "entity.h"


bool        Skybox_Init();
void        Skybox_Destroy();
void        Skybox_Draw();
void        Skybox_SetAng( const glm::vec3& srAng );
void        Skybox_SetMaterial( const std::string& srPath );
const char* Skybox_GetMaterialName();


// Component
struct CSkybox
{
	//ComponentNetVar< std::string > aMaterialPath;

};

