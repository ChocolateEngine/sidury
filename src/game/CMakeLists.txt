message( "Current Project: Sidury Client" )

file(
	GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
	*.cpp
	*.h
	terrain/*.cpp
	terrain/*.h
)

file(
	GLOB_RECURSE PUBLIC_FILES CONFIGURE_DEPENDS
	${CH_PUBLIC}/*.cpp
	${CH_PUBLIC}/*.h
	${CH_PUBLIC}/core/*.h
	${CH_THIRDPARTY}/speedykeyv/KeyValue.cpp
	${CH_THIRDPARTY}/speedykeyv/KeyValue.h
)

#include( ${CH_ROOT}/scripts/joltphysics.cmake )
include( ${CH_ROOT}/scripts/bullet.cmake )
include( ${CH_ROOT}/scripts/vulkan.cmake )

link_libraries( Core )

add_library( SiduryClient SHARED ${SRC_FILES} ${PUBLIC_FILES} )

add_dependencies( SiduryClient "Core" )

set_target_properties(
	SiduryClient PROPERTIES
	RUNTIME_OUTPUT_NAME client
	LIBRARY_OUTPUT_NAME client

	RUNTIME_OUTPUT_DIRECTORY ${CH_BUILD}/sidury/bin
	LIBRARY_OUTPUT_DIRECTORY ${CH_BUILD}/sidury/bin
)

# set output directories for all builds (Debug, Release, etc.)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set_target_properties(
    	SiduryClient PROPERTIES
    	RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CH_BUILD}/sidury/bin
    	LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CH_BUILD}/sidury/bin
    )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

target_precompile_headers( SiduryClient PRIVATE "${CH_PUBLIC}/core/core.h" )

