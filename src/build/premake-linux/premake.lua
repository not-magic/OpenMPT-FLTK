local fltk_config = path.getabsolute("../fltk-linux/bin/fltk-config")
local fltk_cxxflags = (os.outputof(fltk_config .. " --cxxflags") or ""):gsub("%-I/usr/local/include", "")
local fltk_ldflags = (os.outputof(fltk_config .. " --ldflags --use-images") or ""):gsub("%-L/usr/local/lib", "-L" .. path.getabsolute("../fltk-linux/lib"))

workspace "OpenMPT"
	location "../../build/ninja-linux"
	configurations { "Debug", "Release" }
	platforms { "x86_64" }
	architecture "x86_64"
	system "Linux"
	toolset "gcc"

	filter "configurations:Debug"
		defines { "DEBUG", "MPT_BUILD_DEBUG" }
		symbols "On"
	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "Speed"
	filter {}

project "OpenMPT"
	language "C++"
	cppdialect "C++20"
	kind "ConsoleApp"
	targetname "OpenMPT"
	targetdir "../../bin/%{cfg.buildcfg}/ninja-linux"
	objdir "../../build/ninja-linux/obj/%{cfg.buildcfg}"
	warnings "Default"

	defines {
		"MODPLUG_TRACKER",
		"MPT_BUILD_ENABLE_PCH",
		"MPT_WITH_FLTK",
		"__LINUX_ALSA__",
		"MPT_WITH_ZLIB",
		"MPT_WITH_PULSEAUDIO",
		"MPT_WITH_PULSEAUDIOSIMPLE",
		"MPT_WITH_OGG",
		"MPT_WITH_FLAC",
		"FLAC__NO_DLL",
	}
	includedirs {
		"../../build/pch",
		"../../src",
		"../../common",
		"../../soundlib",
		"../../mptrack",
		"../../mptrack/plugins",
		"../../fltk",
		"../../build/fltk-linux",
		"../../include",
		"../../include/pugixml/src",
		"../../include/rtmidi",
		"../../include/portaudio/include",
		"../../include/nlohmann-json/include",
		"../../include/zlib",
		"../../include/SignalsmithStretch",
		"../../include/ogg/include",
		"../../include/ogg/ports/makefile",
		"../../include/flac/include",
	}
	forceincludes { "PCH.h" }
	files {
		"../../src/mpt/**.cpp",
		"../../src/openmpt/**.cpp",
		"../../common/*.cpp",
		"../../soundlib/*.cpp",
		"../../soundlib/plugins/*.cpp",
		"../../soundlib/plugins/dmo/*.cpp",
		"../../sounddsp/*.cpp",
		"../../unarchiver/*.cpp",
		"../../misc/*.cpp",
		"../../tracklib/*.cpp",
		"../../mptrack/*.cpp",
		"../../mptrack/ui/*.cpp",
		"../../mptrack/res/*.cpp",
		"../../mptrack/plugins/*.cpp",
		"../../include/pugixml/src/pugixml.cpp",
		"../../include/rtmidi/RtMidi.cpp",
	}
	removefiles {
		"../../src/mpt/filemode/**",
		"../../src/mpt/main/**",
		"../../src/mpt/terminal/**",
		"../../src/openmpt/sounddevice/SoundDeviceASIO.*",
		"../../src/openmpt/sounddevice/SoundDeviceDirectSound.*",
		"../../src/openmpt/sounddevice/SoundDeviceWaveout.*",
	}
	buildoptions { fltk_cxxflags }
	linkoptions { fltk_ldflags }
	links { "flac", "ogg", "z", "pulse", "pulse-simple", "asound", "pthread", "atomic" }

project "ogg"
	language "C"
	kind "StaticLib"
	targetdir "../../build/ninja-linux/lib/%{cfg.buildcfg}"
	objdir "../../build/ninja-linux/obj/%{cfg.buildcfg}"
	warnings "Off"
	includedirs {
		"../../include/ogg/include",
		"../../include/ogg/ports/makefile",
	}
	files {
		"../../include/ogg/src/bitwise.c",
		"../../include/ogg/src/framing.c",
	}

project "flac"
	language "C"
	kind "StaticLib"
	targetdir "../../build/ninja-linux/lib/%{cfg.buildcfg}"
	objdir "../../build/ninja-linux/obj/%{cfg.buildcfg}"
	warnings "Off"
	defines {
		"FLAC__HAS_OGG=1",
		"FLAC__NO_DLL",
		"FLAC__HAS_X86INTRIN",
		"FLAC__USE_AVX",
		"HAVE_PTHREAD",
		"HAVE_STDINT_H",
		"HAVE_INTTYPES_H",
		"HAVE_CPUID_H",
		"HAVE_LROUND",
		"HAVE_FSEEKO",
		"PACKAGE_VERSION=\"1.5.0\"",
	}
	includedirs {
		"../../include/flac/include",
		"../../include/flac/src/libFLAC/include",
		"../../include/ogg/include",
		"../../include/ogg/ports/makefile",
	}
	files {
		"../../include/flac/src/libFLAC/*.c",
	}
