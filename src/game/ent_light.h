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

	void ComponentUpdated( Entity sEntity, void* spData ) override;
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


// ------------------------------------------------------------


// Really big hack
// What this does is automatically create and update a renderable whenever modelInfo changes
// Maybe I might keep this and merge it into the normal renderable, but I would need to do some changes first
class EntSys_AutoRenderable : public IEntityComponentSystem
{
  public:
	EntSys_AutoRenderable() {}
	~EntSys_AutoRenderable() {}

	void ComponentAdded( Entity sEntity, void* spData ) override;
	void ComponentRemoved( Entity sEntity, void* spData ) override;
	void ComponentUpdated( Entity sEntity, void* spData ) override;
	void Update() override;
};

extern EntSys_AutoRenderable* gEntSys_AutoRenderable[ 2 ];
EntSys_AutoRenderable*        GetAutoRenderableSys();


struct CAutoRenderable
{
	ComponentNetVar< bool > aTestVis    = true;
	ComponentNetVar< bool > aCastShadow = true;
	ComponentNetVar< bool > aVisible    = true;
};


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

