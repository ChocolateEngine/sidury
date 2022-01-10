message( "Current Project: Client" )

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
)

#include( ${CH_ROOT}/scripts/bullet.cmake )
include( ${CH_ROOT}/scripts/vulkan.cmake )

link_libraries( Core )

add_library( Client SHARED ${SRC_FILES} ${PUBLIC_FILES} )

add_dependencies( Client "Core" )

set_target_properties(
	Client PROPERTIES
	OUTPUT_NAME client

	RUNTIME_OUTPUT_DIRECTORY ${CH_BUILD}/sidury/bin
	RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CH_BUILD}/sidury/bin
	RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CH_BUILD}/sidury/bin

	LIBRARY_OUTPUT_DIRECTORY ${CH_BUILD}/sidury/bin
	LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CH_BUILD}/sidury/bin
	LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CH_BUILD}/sidury/bin
)

