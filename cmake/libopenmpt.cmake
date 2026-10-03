# The openmpt Makefile builds in-tree, so the submodule is mirrored into the build directory.
# LIBOPENMPT_BUILD_TEST is upstream's test-suite configuration, the only one that keeps file saving.
# Defines that change type layouts must match the GUI, so they are exported through the imported target.

find_program(RSYNC_EXECUTABLE rsync REQUIRED)
find_program(MAKE_EXECUTABLE NAMES gmake make REQUIRED)
include(ProcessorCount)
ProcessorCount(LIBOPENMPT_JOBS)
if(LIBOPENMPT_JOBS EQUAL 0)
	set(LIBOPENMPT_JOBS 1)
endif()

set(LIBOPENMPT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/openmpt)
set(LIBOPENMPT_EXTRA_CPPFLAGS -DLIBOPENMPT_BUILD_TEST)
# The Makefile must use the same compiler family as the GUI build
if(MINGW)
	set(LIBOPENMPT_MAKE_CONFIG mingw-w64)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set(LIBOPENMPT_MAKE_CONFIG clang)
else()
	set(LIBOPENMPT_MAKE_CONFIG gcc)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(LIBOPENMPT_DEBUG 1)
else()
	set(LIBOPENMPT_DEBUG 0)
endif()

# The Makefile does not rebuild objects when flags change, so each flag set gets its own mirror
string(MD5 LIBOPENMPT_FLAGS_HASH "${LIBOPENMPT_EXTRA_CPPFLAGS};${LIBOPENMPT_DEBUG};${LIBOPENMPT_MAKE_CONFIG}")
string(SUBSTRING ${LIBOPENMPT_FLAGS_HASH} 0 8 LIBOPENMPT_FLAGS_HASH)
set(LIBOPENMPT_MIRROR_DIR ${CMAKE_BINARY_DIR}/libopenmpt-${LIBOPENMPT_FLAGS_HASH})
set(LIBOPENMPT_LIBRARY ${LIBOPENMPT_MIRROR_DIR}/bin/libopenmpt.a)
set(LIBOPENMPT_DEFINES
	LIBOPENMPT_BUILD
	LIBOPENMPT_BUILD_TEST
	MPT_WITH_ZLIB
	MPT_WITH_MINIMP3
	MPT_WITH_STBVORBIS
)

add_custom_target(libopenmpt_make
	COMMAND ${RSYNC_EXECUTABLE} -a --delete --exclude=.git --exclude=*.o --exclude=*.d --exclude=/bin/
		${LIBOPENMPT_SOURCE_DIR}/ ${LIBOPENMPT_MIRROR_DIR}/
	COMMAND ${CMAKE_COMMAND} -E env "CPPFLAGS=${LIBOPENMPT_EXTRA_CPPFLAGS}"
		${MAKE_EXECUTABLE} -C ${LIBOPENMPT_MIRROR_DIR} -j${LIBOPENMPT_JOBS} --no-print-directory
		CONFIG=${LIBOPENMPT_MAKE_CONFIG}
		STDCXX=c++20
		DEBUG=${LIBOPENMPT_DEBUG}
		OPTIMIZE_LTO=0
		SHARED_LIB=0 STATIC_LIB=1 EXAMPLES=0 OPENMPT123=0 TEST=0
		NO_MPG123=1 NO_OGG=1 NO_VORBIS=1 NO_VORBISFILE=1 NO_FLAC=1
		NO_PORTAUDIO=1 NO_PORTAUDIOCPP=1 NO_PULSEAUDIO=1 NO_SDL2=1 NO_SNDFILE=1
		bin/libopenmpt.a
	BYPRODUCTS ${LIBOPENMPT_LIBRARY}
	COMMENT "Building libopenmpt with the openmpt Makefile"
	USES_TERMINAL
	VERBATIM
)

add_library(libopenmpt_engine STATIC IMPORTED GLOBAL)
add_dependencies(libopenmpt_engine libopenmpt_make)
set_target_properties(libopenmpt_engine PROPERTIES IMPORTED_LOCATION ${LIBOPENMPT_LIBRARY})
target_compile_definitions(libopenmpt_engine INTERFACE ${LIBOPENMPT_DEFINES})
if(LIBOPENMPT_DEBUG)
	target_compile_definitions(libopenmpt_engine INTERFACE MPT_BUILD_DEBUG)
endif()
target_include_directories(libopenmpt_engine INTERFACE
	${LIBOPENMPT_SOURCE_DIR}
	${LIBOPENMPT_SOURCE_DIR}/common
	${LIBOPENMPT_SOURCE_DIR}/src
)
target_link_libraries(libopenmpt_engine INTERFACE ZLIB::ZLIB)
