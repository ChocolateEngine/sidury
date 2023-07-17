#pragma once

#include "entity.h"

struct CSuit
{
	// temp
	Handle aLogonSound = CH_INVALID_HANDLE;
};

class SuitSystem : public IEntityComponentSystem
{
public:
	SuitSystem();
	~SuitSystem() {}

	void ComponentAdded( Entity sEntity, void* spData ) override; // Only some will know
	void ComponentRemoved( Entity sEntity, void* spData ) override;
	void Update() override;
};

SuitSystem*        GetSuitEntSys();
