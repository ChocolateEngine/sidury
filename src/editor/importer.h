#pragma once


// NOTE: this will not work for custom importers, like gold source/source engine maps
enum EImportType : u8
{
	EImportType_Invalid,

	EImportType_Texture,
	EImportType_Model,
	
	EImportType_Count,
};


enum EImportStatus : u8
{
	EImportStatus_None,
	EImportStatus_Importing,
	EImportStatus_Finished,
	EImportStatus_Cancelled,
	EImportStatus_Error,

	EImportStatus_Count,
};


enum EImportResult : u8
{
};


struct IImporter
{
	virtual const char* GetName()                            = 0;
	virtual EImportType GetType()                            = 0;

	virtual bool        Init()                               = 0;
	virtual void        Shutdown()                           = 0;

	virtual bool        ImportFile( std::string_view sFile ) = 0;
};


ChVector< IImporter* >& Importer_GetImporters();

bool                    Importer_Init();
void                    Importer_Shutdown();
void                    Importer_Update();

bool                    Importer_StartImport( std::string_view sFile );


struct StaticAssetImporterRegistration
{
	StaticAssetImporterRegistration( IImporter* srImporter )
	{
		Importer_GetImporters().push_back( srImporter );
	}
};


#define CH_REGISTER_ASSET_IMPORTER( srImporter ) static StaticAssetImporterRegistration __gRegister__##srImporter( new srImporter );

