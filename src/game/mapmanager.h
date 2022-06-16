#pragma once

#include "graphics/igraphics.h"

constexpr unsigned int MAP_VERSION = 1;


// matches the names in the keyvalues file
struct MapInfo
{
	unsigned int version = 0;

	std::string mapName;
	std::string modelPath;

	glm::vec3 ang;
	glm::vec3 physAng;

	glm::vec3 spawnPos;
	glm::vec3 spawnAng;

	// unused
	std::string skybox;
};


struct SiduryMap
{
	MapInfo *aMapInfo = nullptr;
	Model *apWorldModel = nullptr;

	DefaultRenderable aRenderable;

#if BULLET_PHYSICS
	std::vector< IPhysicsShape* >  aWorldPhysShapes;
	std::vector< IPhysicsObject* > aWorldPhysObjs;
#endif
};


class MapManager
{
public:
	MapManager();
	~MapManager();

	bool            LoadMap( const std::string& path );
	void            CloseMap();

	void            Update();

	MapInfo        *ParseMapInfo( const std::string &path );
	bool            LoadWorldModel();
	// void            ParseEntities( const std::string &path );
	void            SpawnPlayer();

	glm::vec3       GetSpawnPos();
	glm::vec3       GetSpawnAng();

	SiduryMap      *apMap = nullptr;
};


extern MapManager *mapmanager;

