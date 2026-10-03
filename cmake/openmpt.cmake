set(OPENMPT_UPSTREAM ${CMAKE_CURRENT_SOURCE_DIR}/openmpt)

# Upstream sources the tracker needs that are not part of libopenmpt.a
file(GLOB OPENMPT_UPSTREAM_SOURCES CONFIGURE_DEPENDS
	${OPENMPT_UPSTREAM}/src/openmpt/sounddevice/*.cpp
	${OPENMPT_UPSTREAM}/src/openmpt/soundfile_write/*.cpp
	${OPENMPT_UPSTREAM}/src/openmpt/streamencoder/*.cpp
	${OPENMPT_UPSTREAM}/misc/*.cpp
	${OPENMPT_UPSTREAM}/tracklib/*.cpp
)
if(WIN32)
	list(FILTER OPENMPT_UPSTREAM_SOURCES EXCLUDE REGEX "/sounddevice/SoundDevice(ASIO|DirectSound)\\.")
else()
	list(FILTER OPENMPT_UPSTREAM_SOURCES EXCLUDE REGEX "/sounddevice/SoundDevice(ASIO|DirectSound|Waveout)\\.")
endif()
list(FILTER OPENMPT_UPSTREAM_SOURCES EXCLUDE REGEX "/misc/mptWine\\.")

file(GLOB_RECURSE OPENMPT_GUI_SOURCES CONFIGURE_DEPENDS
	${OPENMPT_FLTK_SRC}/mptrack/*.cpp
	${OPENMPT_FLTK_SRC}/openmpt_ext/*.cpp
)
list(FILTER OPENMPT_GUI_SOURCES EXCLUDE REGEX "/mptrack/manual_generator/")

add_executable(OpenMPT ${OPENMPT_GUI_SOURCES} ${OPENMPT_UPSTREAM_SOURCES})

target_compile_definitions(OpenMPT PRIVATE
	MPT_BUILD_ENABLE_PCH
	MPT_WITH_FLTK
	MPT_WITH_OGG
	$<$<CONFIG:Debug>:DEBUG>
)

target_include_directories(OpenMPT PRIVATE
	${OPENMPT_FLTK_SRC}
	${OPENMPT_FLTK_SRC}/mptrack
	${OPENMPT_FLTK_SRC}/mptrack/plugins
	${OPENMPT_FLTK_SRC}/openmpt_ext/sndlib
	${OPENMPT_UPSTREAM}/soundlib
	${OPENMPT_UPSTREAM}/build/pch
	${OPENMPT_UPSTREAM}/include
	${OPENMPT_UPSTREAM}/include/nlohmann-json/include
	${OPENMPT_UPSTREAM}/include/SignalsmithStretch
)

target_precompile_headers(OpenMPT PRIVATE
	${OPENMPT_UPSTREAM}/build/pch/PCH.h
	<cfloat>
	${OPENMPT_FLTK_SRC}/openmpt_ext/common/BuildSettingsExt.h
	${OPENMPT_FLTK_SRC}/openmpt_ext/common/mptStringExt.h
	${OPENMPT_FLTK_SRC}/openmpt_ext/misc/mptClockExt.h
	${OPENMPT_FLTK_SRC}/openmpt_ext/common/FileSystemExt.h
	${OPENMPT_FLTK_SRC}/openmpt_ext/sndlib/TrackerCriticalSection.h
	${OPENMPT_FLTK_SRC}/openmpt_ext/sndlib/TrackerSoundFile.h
)

target_link_libraries(OpenMPT PRIVATE
	libopenmpt_engine
	fltk::images
	fltk::fltk
	mpt_flac
	mpt_ogg
	mpt_pugixml
	mpt_rtmidi
	mpt_platform
	ZLIB::ZLIB
	Threads::Threads
)
