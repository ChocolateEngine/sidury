#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>


bool Skybox_Init();
void Skybox_Destroy();
void Skybox_Draw();
void Skybox_SetAng( const glm::vec3& srAng );
void Skybox_SetMaterial( const std::string& srPath );

