#include "importer.h"
#include "main.h"

#include "imgui/imgui.h"


#if 0
enum ImageLoadState : char
{
	ImageLoadState_Error = -1,
	ImageLoadState_NotLoaded,
	ImageLoadState_Loading,
	ImageLoadState_Finished,
};


struct ImageLoadThreadData_t
{
	fs::path            aPath;
	ImageInfo*          aInfo = nullptr;  // check to see if this is valid for if it's done loading
	std::vector< char > aData;
	ImageLoadState      aState = ImageLoadState_NotLoaded;
};


// TODO: probably use a mutex lock or something to have this sleep while not used
void LoadImageFunc()
{
	while ( gRunning )
	{
		while ( gLoadThreadQueue.size() )
		{
			auto queueItem = gLoadThreadQueue[ 0 ];
			queueItem->aState = ImageLoadState_Loading;

			if ( !queueItem->aInfo && !queueItem->aPath.empty() )
			{
				queueItem->aPath = fs_clean_path( queueItem->aPath );

				if ( queueItem->aInfo = ImageLoader_LoadImage( queueItem->aPath, queueItem->aData ) )
				{
					gImageHandle      = Render_LoadImage( queueItem->aInfo, queueItem->aData );
					queueItem->aState = ImageLoadState_Finished;
				}
				else
				{
					queueItem->aState = ImageLoadState_Error;
				}
			}

			// if ( gLoadThreadQueue.size() && gLoadThreadQueue[ 0 ] == queueItem )
			// 	gLoadThreadQueue.erase( gLoadThreadQueue.begin() );

			vec_remove( gLoadThreadQueue, queueItem );
		}

		Plat_Sleep( 100 );
	}
}


std::thread gLoadImageThread( LoadImageFunc );


void ImageLoadThread_AddTask( ImageLoadThreadData_t* srData )
{
	gLoadThreadQueue.push_back( srData );
}
#endif


struct ImporterData_t
{
};


struct ImportTask_t
{
};


static ChVector< IImporter* > gImporters;

// TODO: thread this


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


bool Importer_StartImport( std::string_view sFile )
{
	// TODO: Add this to a queue to be run on a thread later
	// right now just do it all here

	return false;
}


CON_COMMAND( importer_list )
{
	Log_MsgF( "Asset Importer Count: %d\n", gImporters.size() );

	for ( u32 i = 0; i < gImporters.size(); i++ )
	{
		Log_MsgF( "    %d - %s\n", i, gImporters[ i ]->GetName() );
	}
}

