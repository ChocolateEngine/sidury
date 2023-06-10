#pragma once

#include "entity.h"


class LightSystem : public IEntityComponentSystem
{
  public:
	LightSystem();
	~LightSystem();

	void ComponentAdded( Entity sEntity ) override;
	void ComponentRemoved( Entity sEntity ) override;
	void ComponentUpdated( Entity sEntity ) override;
	void Update() override;
};

extern LightSystem* gLightEntSystems[ 2 ];
LightSystem*        GetLightEntSys();


// ------------------------------------------------------------


class EntSys_ModelInfo : public IEntityComponentSystem
{
  public:
	EntSys_ModelInfo();
	~EntSys_ModelInfo();

	void ComponentAdded( Entity sEntity ) override;
	void ComponentRemoved( Entity sEntity ) override;
	void ComponentUpdated( Entity sEntity ) override;
	void Update() override;
};

extern EntSys_ModelInfo* gEntSys_ModelInfo[ 2 ];

