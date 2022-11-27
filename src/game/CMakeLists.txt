message( "Current Project: Sidury Client" )

file(
	GLOB_RECURSE SRC_FILES_GLOB CONFIGURE_DEPENDS
	*.cpp
	*.h
	graphics/*.cpp
	graphics/*.h
)

set(
	SRC_FILES
	${SRC_FILES_GLOB}
	
	# tools
	tools/light_editor.cpp
	tools/light_editor.h
)

file(
	GLOB_RECURSE PUBLIC_FILES CONFIGURE_DEPENDS
	${CH_PUBLIC}/*.cpp
	${CH_PUBLIC}/*.h
	${CH_PUBLIC}/core/*.h
)

set(
	THIRDPARTY_FILES
	${CH_THIRDPARTY}/speedykeyv/KeyValue.cpp
	${CH_THIRDPARTY}/speedykeyv/KeyValue.h
)

add_library( SiduryClient SHARED ${SRC_FILES} ${PUBLIC_FILES} ${THIRDPARTY_FILES} )

target_link_libraries(
	SiduryClient
	PRIVATE
	Core
	ImGui
)

add_dependencies( SiduryClient "Core" "ImGui" )

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

source_group(
	TREE ${CMAKE_CURRENT_LIST_DIR}
	PREFIX "Source Files"
	FILES ${SRC_FILES}
)

source_group(
	TREE ${CH_PUBLIC}
	PREFIX "Public"
	FILES ${PUBLIC_FILES}
)

source_group(
	TREE ${CH_THIRDPARTY}
	PREFIX "Thirdparty"
	FILES ${THIRDPARTY_FILES}
)

