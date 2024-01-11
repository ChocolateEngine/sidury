#include "importer.h"
#include "main.h"
#include "core/resource.h"

#include "imgui/imgui.h"

#include <thread>


LOG_REGISTER_CHANNEL2( Importer, LogColor::Default );


struct ImportTask
{
	ImportSettings settings;
	ChHandle_t     handle;
	EImportStatus  status;
};


static ChVector< IImporter* >                          gImporters;
static bool                                            gImporterRunning = true;

static ResourceList< ImportTask* >                     gImportTaskHandles;
static ChVector< ChHandle_t >                          gImportQueue;
static std::unordered_map< ChHandle_t, EImportStatus > gImportStatusMap;


static void Importer_ImportThread()
{
	while ( gImporterRunning )
	{
		if ( gImportQueue.size() )
		{
			ChHandle_t  handle = gImportQueue[ 0 ];
			ImportTask* task   = nullptr;

			if ( !gImportTaskHandles.Get( handle, &task ) )
			{
				gImportQueue.remove( 0 );
				continue;
			}

			// Try each importer
			bool foundImporter = false;
			bool failed        = false;
			for ( u32 i = 0; i < gImporters.size(); i++ )
			{
				std::vector< std::string > exts = gImporters[ i ]->GetExtensions();

				bool                       found = false;
				for ( std::string_view ext : exts )
				{
					if ( str_lower2( task->settings.inputFile.extension().string() ) == ext )
					{
						found = true;
						break;
					}
				}

				if ( !found )
					continue;

				// Found a potential importer, try it out
				foundImporter = true;

				ChVector< ImportResult > output;
				failed = !gImporters[ i ]->ImportFile( task->settings.inputFile, output );

				if ( !failed )
				{
					Log_DevF( gLC_Importer, 1, "Imported File: \"%s\"\n", task->settings.inputFile.c_str() );
					break;
				}
			}

			if ( !foundImporter )
			{
				Log_ErrorF( gLC_Importer, "Failed to find importer for \"%s\"\n", task->settings.inputFile.c_str() );
				gImportStatusMap[ handle ] = EImportStatus_Error;
			}
			else if ( failed )
			{
				Log_ErrorF( gLC_Importer, "Failed to import file: \"%s\"\n", task->settings.inputFile.c_str() );
				gImportStatusMap[ handle ] = EImportStatus_Error;
			}
			else
			{
				gImportStatusMap[ handle ] = EImportStatus_Finished;
			}

			gImportQueue.remove( 0 );
			gImportTaskHandles.Remove( handle );
			delete task;
		}

		sys_sleep( 100 );
	}
}


static std::thread gImportThread( Importer_ImportThread );


ChVector< IImporter* >& Importer_GetImporters()
{
	return gImporters;
}


bool Importer_Init()
{
	for ( u32 i = 0; i < gImporters.size(); i++ )
	{
		// TODO: try to load all importers, then print out what ones failed, like the module loader
		if ( !gImporters[ i ]->Init() )
		{
			Log_ErrorF( "Failed to init asset importer - %s\n", gImporters[ i ]->GetName() );
			return false;
		}
		else
		{
			Log_MsgF( "Loaded Asset Importer - %s\n", gImporters[ i ]->GetName() );
		}
	}

	return true;
}


void Importer_Shutdown()
{
	gImporterRunning = false;
	gImportThread.join();

	for ( u32 i = 0; i < gImporters.size(); i++ )
	{
		gImporters[ i ]->Shutdown();
		delete gImporters[ i ];
	}
	
	gImporters.clear();
}


void Importer_Update()
{
}


// Starts an import task on the import thread
ChHandle_t Importer_StartImport( ImportSettings& srImportSettings )
{
	if ( srImportSettings.inputFile.empty() )
	{
		Log_Error( gLC_Importer, "Empty file in import task\n" );
		return CH_INVALID_HANDLE;
	}

	ImportTask* task  = new ImportTask;
	task->settings    = srImportSettings;
	ChHandle_t handle = gImportTaskHandles.Add( task );

	if ( handle == CH_INVALID_HANDLE )
	{
		Log_Error( gLC_Importer, "Failed to allocate import task\n" );
		return CH_INVALID_HANDLE;
	}

	gImportStatusMap[ handle ] = EImportStatus_Waiting;
	gImportQueue.push_back( handle );

	return handle;
}


// Check the status of an import task
EImportStatus Importer_CheckStatus( ChHandle_t sImportTask )
{
	auto it = gImportStatusMap.find( sImportTask );
	if ( it == gImportStatusMap.end() )
	{
		Log_Error( gLC_Importer, "Failed to find import status\n" );
		return EImportStatus_None;
	}

	EImportStatus status = it->second;

	// if it's finished, remove it from this list
	if ( status == EImportStatus_Finished || status == EImportStatus_Cancelled || status == EImportStatus_Error )
	{
		gImportStatusMap.erase( it );
	}

	return status;
}


// Get the output files of the finished task
std::vector< fs::path > Importer_GetOutputFiles( ChHandle_t sImportTask )
{
	return {};
}


CON_COMMAND( importer_list )
{
	Log_MsgF( "Asset Importer Count: %d\n", gImporters.size() );

	for ( u32 i = 0; i < gImporters.size(); i++ )
	{
		Log_MsgF( "    %d - %s\n", i, gImporters[ i ]->GetName() );
	}
}

