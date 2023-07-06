#pragma once


bool MapConverter_Init();
void MapConverter_Shutdown();
void MapConverter_Update();
void MapConverter_DrawUI();

void MapConverter_ConvertMap( const std::string& srPath, const std::string& srOutPath, const std::string& srAssetPath, const std::string& srAssetsOut );

void MapConverter_ConvertVMF( const std::string& srPath, const std::string& srOutPath, const std::string& srAssetPath, const std::string& srAssetsOut );

