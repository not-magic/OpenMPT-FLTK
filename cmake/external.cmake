set(OPENMPT_FLTK_INCLUDE ${OPENMPT_FLTK_SRC}/include)

add_library(mpt_ogg STATIC
	${OPENMPT_FLTK_INCLUDE}/ogg/src/bitwise.c
	${OPENMPT_FLTK_INCLUDE}/ogg/src/framing.c
)
target_include_directories(mpt_ogg PUBLIC
	${OPENMPT_FLTK_INCLUDE}/ogg/include
	${OPENMPT_FLTK_INCLUDE}/ogg/ports/makefile
)
target_compile_options(mpt_ogg PRIVATE -w)

file(GLOB MPT_FLAC_SOURCES CONFIGURE_DEPENDS ${OPENMPT_FLTK_INCLUDE}/flac/src/libFLAC/*.c)
add_library(mpt_flac STATIC ${MPT_FLAC_SOURCES})
target_compile_definitions(mpt_flac
	PUBLIC
		FLAC__NO_DLL
	PRIVATE
		FLAC__HAS_OGG=1
		FLAC__HAS_X86INTRIN
		FLAC__USE_AVX
		HAVE_PTHREAD
		HAVE_STDINT_H
		HAVE_INTTYPES_H
		HAVE_CPUID_H
		HAVE_LROUND
		HAVE_FSEEKO
		PACKAGE_VERSION="1.5.0"
)
target_include_directories(mpt_flac
	PUBLIC
		${OPENMPT_FLTK_INCLUDE}/flac/include
	PRIVATE
		${OPENMPT_FLTK_INCLUDE}/flac/src/libFLAC/include
)
target_link_libraries(mpt_flac PUBLIC mpt_ogg)
target_compile_options(mpt_flac PRIVATE -w)

add_library(mpt_pugixml STATIC ${OPENMPT_FLTK_INCLUDE}/pugixml/src/pugixml.cpp)
target_include_directories(mpt_pugixml PUBLIC ${OPENMPT_FLTK_INCLUDE}/pugixml/src)
target_compile_options(mpt_pugixml PRIVATE -w)

find_package(ALSA REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PULSE REQUIRED IMPORTED_TARGET libpulse libpulse-simple)

add_library(mpt_rtmidi STATIC ${OPENMPT_FLTK_INCLUDE}/rtmidi/RtMidi.cpp)
target_include_directories(mpt_rtmidi PUBLIC ${OPENMPT_FLTK_INCLUDE}/rtmidi)
target_compile_definitions(mpt_rtmidi PUBLIC __LINUX_ALSA__)
target_link_libraries(mpt_rtmidi PUBLIC ALSA::ALSA Threads::Threads)
target_compile_options(mpt_rtmidi PRIVATE -w)
