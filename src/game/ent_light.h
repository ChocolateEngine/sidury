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


// ------------------------------------------------------------


class EntSys_Transform : public IEntityComponentSystem
{
  public:
	EntSys_Transform() {}
	~EntSys_Transform() {}

	void ComponentUpdated( Entity sEntity, void* spData ) override;
	void Update() override;
};

extern EntSys_Transform* gEntSys_Transform[ 2 ];


// ------------------------------------------------------------


// ------------------------------------------------------------


// Really big hack
// What this does is automatically create and update a renderable whenever modelInfo changes
// Maybe I might keep this and merge it into the normal renderable, but I would need to do some changes first
class EntSys_Renderable : public IEntityComponentSystem
{
  public:
	EntSys_Renderable() {}
	~EntSys_Renderable() {}

	void ComponentAdded( Entity sEntity, void* spData ) override;
	void ComponentRemoved( Entity sEntity, void* spData ) override;
	void ComponentUpdated( Entity sEntity, void* spData ) override;
	void Update() override;
};

extern EntSys_Renderable* gEntSys_Renderable[ 2 ];
EntSys_Renderable*        GetRenderableEntSys();


// ------------------------------------------------------------


// Really big hack part 2
// What this does is automatically create and update a physics object whenever physInfo changes
class EntSys_PhysInfo : public IEntityComponentSystem
{
  public:
	EntSys_PhysInfo() {}
	~EntSys_PhysInfo() {}

	void ComponentAdded( Entity sEntity, void* spData ) override;
	void ComponentRemoved( Entity sEntity, void* spData ) override;
	void ComponentUpdated( Entity sEntity, void* spData ) override;
	void Update() override;
};

extern EntSys_PhysInfo* gEntSys_PhysInfo[ 2 ];


struct CPhysInfo
{
};

