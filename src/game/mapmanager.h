#pragma once

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
	ModelDraw_t aRenderable{};

	std::vector< IPhysicsShape* >  aWorldPhysShapes;
	std::vector< IPhysicsObject* > aWorldPhysObjs;
};


bool      MapManager_LoadMap( const std::string& path );
void      MapManager_CloseMap();
bool      MapManager_HasMap();

void      MapManager_Update();

MapInfo*  MapManager_ParseMapInfo( const std::string& path );
bool      MapManager_LoadWorldModel();
// void            MapManager_ParseEntities( const std::string &path );
void      MapManager_SpawnPlayer();

glm::vec3 MapManager_GetSpawnPos();
glm::vec3 MapManager_GetSpawnAng();


