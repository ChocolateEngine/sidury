message( "Current Project: Sidury Client" )

file(
	GLOB_RECURSE SRC_FILES_GLOB CONFIGURE_DEPENDS
	*.cpp
	*.h
)

set(
	SRC_FILES
	${SRC_FILES_GLOB}
	
	# networking
	network/net_main.cpp
	network/net_main.h
	
	# flatbuffers
	flatbuffers/sidury.capnp.c++
	flatbuffers/sidury.capnp.h
	
	# tools
	tools/light_editor.cpp
	tools/light_editor.h
)

if( WIN32 )
	set(
		SRC_FILES
		${SRC_FILES_GLOB}
		
		# networking
		network/net_windows.cpp
		network/net_windows.h
	)
endif()

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
)

set( FLATBUFFERS_DIR_DEBUG "${CH_THIRDPARTY}/flatbuffers/build/Debug" )
set( FLATBUFFERS_DIR_RELEASE "${CH_THIRDPARTY}/flatbuffers/build/Release" )

link_directories(
	"$<$<CONFIG:Debug>:${FLATBUFFERS_DIR_DEBUG}>"
	"$<$<CONFIG:Release>:${FLATBUFFERS_DIR_RELEASE}>"
	"$<$<CONFIG:RelWithDebInfo>:${FLATBUFFERS_DIR_RELEASE}>"
)

# include( ${CH_ROOT}/scripts/rmlui.cmake )

# ??????
add_library( SiduryClient SHARED ../graphics/mesh_builder.cpp ${SRC_FILES} ${PUBLIC_FILES} ${THIRDPARTY_FILES} )

target_link_libraries(
	SiduryClient
	PRIVATE
	Core
	ImGui
	flatbuffers
)

if( WIN32 )
	target_link_libraries(
		SiduryClient
		PRIVATE
		# Networking
		wsock32
		ws2_32
		Iphlpapi
	)
endif()

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
    	RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CH_BUILD}/sidury/bin/${PLAT_FOLDER}
    	LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CH_BUILD}/sidury/bin/${PLAT_FOLDER}
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

