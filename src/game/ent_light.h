#pragma once

#include "entity.h"


class LightSystem : public IEntityComponentSystem
{
  public:
	LightSystem() {}
	~LightSystem() {}

	void ComponentAdded( Entity sEntity, void* spData ) override;
	void ComponentRemoved( Entity sEntity, void* spData ) override;
	void ComponentUpdated( Entity sEntity, void* spData ) override;
	void Update() override;
};

extern LightSystem* gLightEntSystems[ 2 ];
LightSystem*        GetLightEntSys();


// ------------------------------------------------------------


class EntSys_ModelInfo : public IEntityComponentSystem
{
  public:
	EntSys_ModelInfo() {}
	~EntSys_ModelInfo() {}

	void ComponentAdded( Entity sEntity, void* spData ) override;
	void ComponentRemoved( Entity sEntity, void* spData ) override;
	void ComponentUpdated( Entity sEntity, void* spData ) override;
	void Update() override;
};

extern EntSys_ModelInfo* gEntSys_ModelInfo[ 2 ];


// ------------------------------------------------------------


class EntSys_Transform : public IEntityComponentSystem
{
  public:
	EntSys_Transform() {}
	~EntSys_Transform() {}

	void Update() override;
};

extern EntSys_Transform* gEntSys_Transform[ 2 ];


// ------------------------------------------------------------


class EntSys_Renderable : public IEntityComponentSystem
{
  public:
	EntSys_Renderable() {}
	~EntSys_Renderable() {}

	void ComponentRemoved( Entity sEntity, void* spData ) override;
};

extern EntSys_Renderable* gEntSys_Renderable[ 2 ];


