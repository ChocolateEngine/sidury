message( "Current Project: Sidury Editor" )

file(
	GLOB_RECURSE SRC_FILES_GLOB CONFIGURE_DEPENDS
	*.cpp
	*.h
	../shared/*.cpp
	../shared/*.h
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
	${CH_THIRDPARTY}/speedykeyv/KeyValue.cpp
	${CH_THIRDPARTY}/speedykeyv/KeyValue.h
)

include_directories(
	"${CH_THIRDPARTY}/flatbuffers/include"
	"../graphics"
	"../shared"

	# Import Formats
	"${CH_THIRDPARTY}/mozjpeg"
	"${CH_THIRDPARTY}/mozjpeg/build"
)

set( FLATBUFFERS_DIR_DEBUG "${CH_THIRDPARTY}/flatbuffers/build/Debug" )
set( FLATBUFFERS_DIR_RELEASE "${CH_THIRDPARTY}/flatbuffers/build/Release" )

set( MOZJPEG_DIR_DEBUG "${CH_THIRDPARTY}/mozjpeg/build/Debug" )
set( MOZJPEG_DIR_RELEASE "${CH_THIRDPARTY}/mozjpeg/build/Release" )

link_directories(
	"$<$<CONFIG:Debug>:${FLATBUFFERS_DIR_DEBUG}>"
	"$<$<CONFIG:Release>:${FLATBUFFERS_DIR_RELEASE}>"
	"$<$<CONFIG:RelWithDebInfo>:${FLATBUFFERS_DIR_RELEASE}>"
	"$<$<CONFIG:Profile>:${FLATBUFFERS_DIR_RELEASE}>"

	"$<$<CONFIG:Debug>:${MOZJPEG_DIR_DEBUG}>"
	"$<$<CONFIG:Release>:${MOZJPEG_DIR_RELEASE}>"
	"$<$<CONFIG:RelWithDebInfo>:${MOZJPEG_DIR_RELEASE}>"
	"$<$<CONFIG:Profile>:${MOZJPEG_DIR_RELEASE}>"

	"${CH_THIRDPARTY}/flatbuffers/build"
	"${CH_THIRDPARTY}/mozjpeg/build"
)

# include( ${CH_ROOT}/scripts/rmlui.cmake )

add_library( Editor SHARED ${SRC_FILES} ${PUBLIC_FILES} ${THIRDPARTY_FILES} )

target_link_libraries(
	Editor
	PRIVATE
	Core
	ImGui
	SDL2
	flatbuffers
)

# TODO: will be moved to asset importer/exporter dll
if( MSVC )
	target_link_libraries(
		Editor
		PRIVATE
		jpeg-static
		turbojpeg-static
	)
else()
	target_link_libraries(
		Editor
		PRIVATE
		jpeg
		turbojpeg
	)
endif()

add_dependencies( Editor "Core" "ImGui" )

set_target_properties(
	Editor PROPERTIES
	RUNTIME_OUTPUT_NAME editor
	LIBRARY_OUTPUT_NAME editor

	RUNTIME_OUTPUT_DIRECTORY ${CH_BUILD}/sidury/bin/${PLAT_FOLDER}
	LIBRARY_OUTPUT_DIRECTORY ${CH_BUILD}/sidury/bin/${PLAT_FOLDER}
)

# set output directories for all builds (Debug, Release, etc.)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set_target_properties(
    	Editor PROPERTIES
    	RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CH_BUILD}/sidury/bin/${PLAT_FOLDER}
    	LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CH_BUILD}/sidury/bin/${PLAT_FOLDER}
    )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

target_precompile_headers( Editor PRIVATE "${CH_PUBLIC}/core/core.h" )

source_group(
	TREE ${CMAKE_CURRENT_LIST_DIR}/../
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

