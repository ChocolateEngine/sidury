message( "Current Project: Launcher" )

set(
	SRC_FILES
	gameloader.c
	${CH_PUBLIC}/core/platform.c  # temp
)

#file(
#	GLOB_RECURSE PUBLIC_FILES CONFIGURE_DEPENDS
#	${CH_PUBLIC}/*.cpp
#	${CH_PUBLIC}/*.h
#	${CH_PUBLIC}/core/*.h
#)

#add_executable( Launcher ${SRC_FILES} ${PUBLIC_FILES} )
add_executable( Launcher ${SRC_FILES} )

set_target_properties(
	Launcher PROPERTIES
	OUTPUT_NAME sidury
	RUNTIME_OUTPUT_DIRECTORY ${GAME_DIR}
	RUNTIME_OUTPUT_DIRECTORY_DEBUG ${GAME_DIR}
	RUNTIME_OUTPUT_DIRECTORY_RELEASE ${GAME_DIR}
)

# Set as startup project in visual studio
set_property( DIRECTORY ${CMAKE_CURRENT_LIST_DIR} PROPERTY VS_STARTUP_PROJECT Launcher )

