cmake_minimum_required( VERSION 3.18 FATAL_ERROR )

set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

set( GAME_DIR ${CMAKE_CURRENT_LIST_DIR}/sidury )

project( Sidury )

# Add Chocolate Framework DLLs
add_subdirectory( chocolate )

# grab the globals and put them here
get_property( CH_ROOT GLOBAL PROPERTY CH_ROOT )
get_property( CH_PUBLIC GLOBAL PROPERTY CH_PUBLIC )
get_property( CH_THIRDPARTY GLOBAL PROPERTY CH_THIRDPARTY )

include( ${CH_ROOT}/scripts/core.cmake )

# Now Add Sidury Projects
add_subdirectory( src/launcher )
add_subdirectory( src/game )

