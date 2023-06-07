#pragma once

#include "entity.h"


class LightSystem : public IEntityComponentSystem
{
  public:
	LightSystem();
	~LightSystem();

	void ComponentAdded( Entity sEntity ) override;
	void ComponentRemoved( Entity sEntity ) override;
	void Update() override;
};

extern LightSystem* gLightEntSystems[ 2 ];

