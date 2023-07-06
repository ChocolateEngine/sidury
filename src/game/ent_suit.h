#pragma once

#include "entity.h"

struct CSuit
{
};

class SuitSystem : public IEntityComponentSystem
{
public:
	SuitSystem();
	~SuitSystem() {}

	void ComponentAdded( Entity sEntity, void* spData ) override; // Only some will know

private:
	Handle aLogonSound = CH_INVALID_HANDLE;
};

SuitSystem*        GetSuitEntSys();
