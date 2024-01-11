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
	EImportStatus_Waiting,
	EImportStatus_Importing,
	EImportStatus_Finished,
	EImportStatus_Cancelled,
	EImportStatus_Error,

	EImportStatus_Count,
};


enum EImportResult : u8
{
	EImportResult_Failed,
	EImportResult_Success,
};


struct ImportResult
{
	EImportType type;
	ChHandle_t  handle;
};


struct ImportSettings
{
	fs::path      inputFile;

	// For example, if we import an image, load it as a texture
	// (DOES NOT WORK ON COMMAND LINE VERSION)
	bool          loadAsset   = true;

	// Export the assets to a directory
	bool          exportFiles = true;
	fs::path      outputDirectory;
};


struct IImporter
{
	virtual const char*                GetName()                                                                = 0;
	virtual EImportType                GetType()                                                                = 0;
	virtual std::vector< std::string > GetExtensions()                                                          = 0;

	virtual bool                       Init()                                                                   = 0;
	virtual void                       Shutdown()                                                               = 0;

	virtual bool                       ImportFile( fs::path sFile, ChVector< ImportResult >& srOutput )         = 0;

	// virtual bool                       ExportFile( fs::path sFile, ChVector< ImportResult >& srOutput )         = 0;
};




// use this when this is a separate DLL
// mainly so this can be used in the game code if you want a spray, or use the importer on the command line
// struct IImporterSystem : public ISystem


ChVector< IImporter* >& Importer_GetImporters();

bool                    Importer_Init();
void                    Importer_Shutdown();
void                    Importer_Update();

// Starts an import task on the import thread
ChHandle_t              Importer_StartImport( ImportSettings& srImportSettings );

// Check the status of an import task
EImportStatus           Importer_CheckStatus( ChHandle_t sImportTask );

// Get the output files of the finished task
std::vector< fs::path > Importer_GetOutputFiles( ChHandle_t sImportTask );


struct StaticAssetImporterRegistration
{
	StaticAssetImporterRegistration( IImporter* srImporter )
	{
		Importer_GetImporters().push_back( srImporter );
	}
};


#define CH_REGISTER_ASSET_IMPORTER( srImporter ) static StaticAssetImporterRegistration __gRegister__##srImporter( new srImporter );

#define CH_IMPORTER_INTERFACE_VER                1

#define CH_IMPORTER_SYSTEM_NAME                  "ImporterSystem"
#define CH_IMPORTER_SYSTEM_VER                   1

