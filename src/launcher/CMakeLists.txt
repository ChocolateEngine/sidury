message( "Current Project: Sidury Launcher" )

set(
	SRC_FILES
	gameloader.c
	${CH_PUBLIC}/core/platform.c  # temp
)

add_executable( SiduryLauncher ${SRC_FILES} )

set_target_properties(
	SiduryLauncher PROPERTIES
	OUTPUT_NAME sidurylauncher
	RUNTIME_OUTPUT_DIRECTORY ${CH_BUILD}
	RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CH_BUILD}
	RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CH_BUILD}
	
	VS_DEBUGGER_WORKING_DIRECTORY ${CH_BUILD}
)

# Set as startup project in visual studio
set_property( DIRECTORY ${CMAKE_CURRENT_LIST_DIR} PROPERTY VS_STARTUP_PROJECT SiduryLauncher )

