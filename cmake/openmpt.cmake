file(GLOB_RECURSE OPENMPT_SOURCES CONFIGURE_DEPENDS
	${OPENMPT_FLTK_SRC}/src/mpt/*.cpp
	${OPENMPT_FLTK_SRC}/src/openmpt/*.cpp
)
list(FILTER OPENMPT_SOURCES EXCLUDE REGEX "/src/mpt/(filemode|main|terminal)/")
list(FILTER OPENMPT_SOURCES EXCLUDE REGEX "/sounddevice/SoundDevice(ASIO|DirectSound|Waveout)\\.")

file(GLOB OPENMPT_GUI_SOURCES CONFIGURE_DEPENDS
	${OPENMPT_FLTK_SRC}/common/*.cpp
	${OPENMPT_FLTK_SRC}/soundlib/*.cpp
	${OPENMPT_FLTK_SRC}/soundlib/plugins/*.cpp
	${OPENMPT_FLTK_SRC}/soundlib/plugins/dmo/*.cpp
	${OPENMPT_FLTK_SRC}/sounddsp/*.cpp
	${OPENMPT_FLTK_SRC}/unarchiver/*.cpp
	${OPENMPT_FLTK_SRC}/misc/*.cpp
	${OPENMPT_FLTK_SRC}/tracklib/*.cpp
	${OPENMPT_FLTK_SRC}/mptrack/*.cpp
	${OPENMPT_FLTK_SRC}/mptrack/ui/*.cpp
	${OPENMPT_FLTK_SRC}/mptrack/res/*.cpp
	${OPENMPT_FLTK_SRC}/mptrack/plugins/*.cpp
)

add_executable(OpenMPT ${OPENMPT_SOURCES} ${OPENMPT_GUI_SOURCES})

target_compile_definitions(OpenMPT PRIVATE
	MODPLUG_TRACKER
	MPT_BUILD_ENABLE_PCH
	MPT_WITH_FLTK
	MPT_WITH_ZLIB
	MPT_WITH_PULSEAUDIO
	MPT_WITH_PULSEAUDIOSIMPLE
	MPT_WITH_OGG
	MPT_WITH_FLAC
	$<$<CONFIG:Debug>:DEBUG>
	$<$<CONFIG:Debug>:MPT_BUILD_DEBUG>
)

target_include_directories(OpenMPT PRIVATE
	${OPENMPT_FLTK_SRC}/build/pch
	${OPENMPT_FLTK_SRC}/src
	${OPENMPT_FLTK_SRC}/common
	${OPENMPT_FLTK_SRC}/soundlib
	${OPENMPT_FLTK_SRC}/mptrack
	${OPENMPT_FLTK_SRC}/mptrack/plugins
	${OPENMPT_FLTK_SRC}/include
	${OPENMPT_FLTK_SRC}/include/nlohmann-json/include
	${OPENMPT_FLTK_SRC}/include/SignalsmithStretch
)

target_precompile_headers(OpenMPT PRIVATE ${OPENMPT_FLTK_SRC}/build/pch/PCH.h)

target_link_libraries(OpenMPT PRIVATE
	fltk::images
	fltk::fltk
	mpt_flac
	mpt_ogg
	mpt_pugixml
	mpt_rtmidi
	PkgConfig::PULSE
	ALSA::ALSA
	ZLIB::ZLIB
	Threads::Threads
	atomic
)
