message( "Current Project: Sidury Launcher" )

set(
	SRC_FILES
	gameloader.cpp
)

add_executable( SiduryLauncher ${SRC_FILES} )

set( SIDURY_LAUNCHER_NAME sidurylauncher )

set_target_properties(
	SiduryLauncher PROPERTIES
	OUTPUT_NAME ${SIDURY_LAUNCHER_NAME}
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

if (WIN32)
	# Copy a pre-built manifest file to the output directory for DPI Scaling
	add_custom_command(
		TARGET SiduryLauncher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/${SIDURY_LAUNCHER_NAME}.exe.manifest" "${CH_BUILD}/${SIDURY_LAUNCHER_NAME}.exe.manifest"
		COMMAND_EXPAND_LISTS
        COMMENT "Copying ${SIDURY_LAUNCHER_NAME}.exe.manifest to launcher directory"
    )
endif(WIN32)

