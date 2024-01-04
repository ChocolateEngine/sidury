message( "Current Project: Chocolate UI Test DLL" )

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
)

file(
	GLOB_RECURSE PUBLIC_FILES CONFIGURE_DEPENDS
	${CH_PUBLIC}/*.cpp
	${CH_PUBLIC}/*.h
	${CH_PUBLIC}/core/*.h
)

set(
	THIRDPARTY_FILES
)

include_directories( ../chui/public )

add_library( ChUI_Test SHARED ${SRC_FILES} ${PUBLIC_FILES} ${THIRDPARTY_FILES} )

target_link_libraries(
	ChUI_Test
	PRIVATE
	Core
	ImGui
	ChocolateUI
)

add_dependencies( ChUI_Test "Core" "ImGui" "ChocolateUI" )

set_target_properties(
	ChUI_Test PROPERTIES
	RUNTIME_OUTPUT_NAME chui_app
	LIBRARY_OUTPUT_NAME chui_app

	RUNTIME_OUTPUT_DIRECTORY ${CH_BUILD}/chui_app/bin
	LIBRARY_OUTPUT_DIRECTORY ${CH_BUILD}/chui_app/bin
)

# set output directories for all builds (Debug, Release, etc.)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set_target_properties(
    	ChUI_Test PROPERTIES
    	RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CH_BUILD}/chui_app/bin
    	LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CH_BUILD}/chui_app/bin
    )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

target_precompile_headers( ChUI_Test PRIVATE "${CH_PUBLIC}/core/core.h" )

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

