cmake_minimum_required( VERSION 3.18 FATAL_ERROR )

set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

project( Sidury )

option( STEAM "Build the steam abstraction" OFF )

# Add Chocolate Framework DLLs
# add_subdirectory( ../chocolate ${CMAKE_CURRENT_LIST_DIR} )
add_subdirectory( ${CMAKE_CURRENT_LIST_DIR}/../chocolate ${CMAKE_CURRENT_LIST_DIR}/../chocolate )

# grab the globals and put them here
get_property( CH_ROOT GLOBAL PROPERTY CH_ROOT )
get_property( CH_PUBLIC GLOBAL PROPERTY CH_PUBLIC )
get_property( CH_THIRDPARTY GLOBAL PROPERTY CH_THIRDPARTY )

include( ${CH_ROOT}/scripts/core.cmake )

set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CH_ROOT}/obj/sidury )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CH_ROOT}/obj/sidury/Debug )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CH_ROOT}/obj/sidury/Release )

# Now Add Sidury Projects
add_subdirectory( src/launcher )

# add_subdirectory( src/game )
# add_subdirectory( src/sidury2 )

add_subdirectory( src/editor )
add_subdirectory( src/graphics )

if( STEAM )
	add_subdirectory( src/steam )
endif( STEAM)

