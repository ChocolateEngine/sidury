message( "Current Project: Sidury Launcher" )

set(
	GAME_SRC_FILES
	launcher_base.cpp
	launcher_base.h

	launcher_sidury.cpp
)

add_executable( LauncherSidury ${GAME_SRC_FILES} )

set( SIDURY_LAUNCHER_NAME sidury )

set_target_properties(
	LauncherSidury PROPERTIES
	OUTPUT_NAME ${SIDURY_LAUNCHER_NAME}_${PLAT_FOLDER}
	RUNTIME_OUTPUT_DIRECTORY ${CH_BUILD}
	
	VS_DEBUGGER_WORKING_DIRECTORY ${CH_BUILD}
)

# set output directories for all builds (Debug, Release, etc.)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set_target_properties(
    	LauncherSidury PROPERTIES
    	RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CH_BUILD}
    )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

# Set as startup project in visual studio
# set_property( DIRECTORY ${CMAKE_CURRENT_LIST_DIR} PROPERTY VS_STARTUP_PROJECT LauncherSidury )

if (MSVC)
	# Copy a pre-built manifest file to the output directory for DPI Scaling
	add_custom_command(
		TARGET LauncherSidury POST_BUILD
        # COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/launcher.exe.manifest" "${CH_BIN}/${SIDURY_LAUNCHER_NAME}_${PLAT_FOLDER}.exe.manifest"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/launcher.exe.manifest" "${CH_BUILD}/${SIDURY_LAUNCHER_NAME}_${PLAT_FOLDER}.exe.manifest"
		COMMAND_EXPAND_LISTS
        COMMENT "Copying ${SIDURY_LAUNCHER_NAME}.exe.manifest to launcher directory"
    )
endif(MSVC)


# ======================================================================================================


message( "Current Project: Editor Launcher" )

set(
	EDITOR_SRC_FILES
	launcher_base.cpp
	launcher_base.h

	launcher_editor.cpp
)

add_executable( LauncherEditor ${EDITOR_SRC_FILES} )

set( EDITOR_LAUNCHER_NAME editor )

set_target_properties(
	LauncherEditor PROPERTIES
	OUTPUT_NAME ${EDITOR_LAUNCHER_NAME}_${PLAT_FOLDER}
	RUNTIME_OUTPUT_DIRECTORY ${CH_BUILD}
	
	VS_DEBUGGER_WORKING_DIRECTORY ${CH_BUILD}
)

# set output directories for all builds (Debug, Release, etc.)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set_target_properties(
    	LauncherEditor PROPERTIES
    	RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CH_BUILD}
    )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

# Set as startup project in visual studio
# set_property( DIRECTORY ${CMAKE_CURRENT_LIST_DIR} PROPERTY VS_STARTUP_PROJECT LauncherEditor )

if (MSVC)
	# Copy a pre-built manifest file to the output directory for DPI Scaling
	add_custom_command(
		TARGET LauncherEditor POST_BUILD
        # COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/launcher.exe.manifest" "${CH_BIN}/${EDITOR_LAUNCHER_NAME}_${PLAT_FOLDER}.exe.manifest"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/launcher.exe.manifest" "${CH_BUILD}/${EDITOR_LAUNCHER_NAME}_${PLAT_FOLDER}.exe.manifest"
		COMMAND_EXPAND_LISTS
        COMMENT "Copying ${EDITOR_LAUNCHER_NAME}.exe.manifest to launcher directory"
    )
endif(MSVC)


# ======================================================================================================


message( "Current Project: Toolkit Launcher" )

set(
	EDITOR_SRC_FILES
	launcher_base.cpp
	launcher_base.h

	launcher_toolkit.cpp
)

add_executable( LauncherToolkit ${EDITOR_SRC_FILES} )

set( TOOLKIT_LAUNCHER_NAME toolkit )

set_target_properties(
	LauncherToolkit PROPERTIES
	OUTPUT_NAME ${TOOLKIT_LAUNCHER_NAME}_${PLAT_FOLDER}
	RUNTIME_OUTPUT_DIRECTORY ${CH_BUILD}
	
	VS_DEBUGGER_WORKING_DIRECTORY ${CH_BUILD}
)

# set output directories for all builds (Debug, Release, etc.)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set_target_properties(
    	LauncherToolkit PROPERTIES
    	RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CH_BUILD}
    )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

# Set as startup project in visual studio
# set_property( DIRECTORY ${CMAKE_CURRENT_LIST_DIR} PROPERTY VS_STARTUP_PROJECT LauncherToolkit )

if (MSVC)
	# Copy a pre-built manifest file to the output directory for DPI Scaling
	add_custom_command(
		TARGET LauncherToolkit POST_BUILD
        # COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/launcher.exe.manifest" "${CH_BIN}/${TOOLKIT_LAUNCHER_NAME}_${PLAT_FOLDER}.exe.manifest"
        COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_LIST_DIR}/launcher.exe.manifest" "${CH_BUILD}/${TOOLKIT_LAUNCHER_NAME}_${PLAT_FOLDER}.exe.manifest"
		COMMAND_EXPAND_LISTS
        COMMENT "Copying ${TOOLKIT_LAUNCHER_NAME}.exe.manifest to launcher directory"
    )
endif(MSVC)


