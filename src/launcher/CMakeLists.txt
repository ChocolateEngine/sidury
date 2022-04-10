message( "Current Project: Sidury Launcher" )

set(
	SRC_FILES
	gameloader.c
)

add_executable( SiduryLauncher ${SRC_FILES} )

set_target_properties(
	SiduryLauncher PROPERTIES
	OUTPUT_NAME sidurylauncher
	RUNTIME_OUTPUT_DIRECTORY ${CH_BUILD}
	
	VS_DEBUGGER_WORKING_DIRECTORY ${CH_BUILD}
)

# set output directories for all builds (Debug, Release, etc.)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set_target_properties(
    	SiduryLauncher PROPERTIES
    	RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CH_BUILD}
    )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

# Set as startup project in visual studio
set_property( DIRECTORY ${CMAKE_CURRENT_LIST_DIR} PROPERTY VS_STARTUP_PROJECT SiduryLauncher )

