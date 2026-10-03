# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

OpenMPT-FLTK is a cross-platform port of the OpenMPT tracker GUI (`mptrack`) from MFC to FLTK.

- `openmpt/` — submodule, upstream OpenMPT trunk (`master`). Never edit it. Use `openmpt/mptrack/` as the reference for how the Windows GUI behaves.
- `fltk/` — submodule, FLTK `branch-1.4`. The port must stick to the 1.4 API (no `Fl_Slider::value_to_position`, `Fl_Window::shown()` is non-const, ...).
- `src/mptrack/` — the FLTK GUI. `src/mptrack/ui/` is the MFC-replacement toolkit (namespace `ui`, `Wnd`/`WndT<FlBase>`, message maps, `Painter` for GDI). `src/mptrack/res/ResourceData.cpp` is generated from `mptrack.rc` by `src/tools/rc2cpp.py` (run manually, output is committed).
- `src/openmpt_ext/` — tracker-only engine functionality that libopenmpt does not build (see below). Prefer compiling upstream files over copying them: when a file is only disabled by a guard macro that no engine type layout depends on, include it with the guard lifted (see `sounddsp/SoundDspExt.h`). Copy only when upstream's version cannot compile here, e.g. because it includes `openmpt/mptrack` (MFC) headers or defines symbols libopenmpt already defines.
- `cmake/` — the build (`fltk.cmake`, `platform.cmake` for per-OS audio/MIDI backends, `external.cmake` for ogg/flac/pugixml/rtmidi from `openmpt/include`, `libopenmpt.cmake`, `openmpt.cmake` for the GUI).

The port must stay buildable on Windows and macOS with FLTK, even though only Linux is tested. Do not fill gaps in upstream's `mpt` namespace only for non-Windows (upstream's Windows-only code there usually does not compile in library mode either). Use the OS-agnostic helpers instead: `FileSystem::` (`openmpt_ext/common/FileSystemExt.h`) instead of `mpt::native_fs`/`mpt::common_directories`, and `AtomicSharedFile` instead of `mpt::IO::atomic_shared_file_ref`. Keep platform `#if`s inside their implementations.

Upstream does not accept LLM-assisted contributions, so fixes to `openmpt/` cannot be upstreamed; work around upstream issues in `src/`.

## Build

```
cmake -S . -B build -G Ninja      # Debug by default
ninja -C build OpenMPT            # produces build/bin/OpenMPT
build/bin/OpenMPT -play <module>  # opens and starts playing a module
```

- `libopenmpt_make` mirrors `openmpt/` into `build/libopenmpt-<hash>/` with rsync and runs the upstream Makefile there (it builds in-tree). The hash covers the flags, because the Makefile does not rebuild objects when flags change.
- PCH gotcha: CMake rewrites `build/CMakeFiles/OpenMPT.dir/cmake_pch.hxx` without updating its timestamp. After changing the `target_precompile_headers` list, delete `cmake_pch.hxx.gch`.

## How the GUI uses libopenmpt

The GUI links `libopenmpt.a` built by the upstream Makefile and compiles against `openmpt/` headers in library mode (`LIBOPENMPT_BUILD`, not `MODPLUG_TRACKER`). Rules that follow from that:

- **Type layouts must match.** Every define that affects engine headers must be identical for libopenmpt and the GUI. They are exported by the `libopenmpt_engine` imported target: `LIBOPENMPT_BUILD`, `LIBOPENMPT_BUILD_TEST`, `MPT_BUILD_DEBUG`, `MPT_WITH_ZLIB/MINIMP3/STBVORBIS`, C++20, no LTO. Never define `MPT_WITH_FLAC` in the GUI (BuildSettings.h errors) or anything that changes `CSoundFile`, `ModChannel`, `IMixPlugin`, `PathString`, etc.
- **`LIBOPENMPT_BUILD_TEST`** is upstream's test-suite configuration. It is the only library configuration that keeps file saving (`SaveIT`, `SaveXM`, sample savers), and it also enables `ENABLE_TESTS`, playback traces and locale charsets. The GUI provides `AssertHandler` (Main.cpp).
- **Namespace.** Library mode puts everything in namespace `OpenMPT` (the tracker build has none). Code outside `OPENMPT_NAMESPACE_BEGIN` cannot see engine types, forward declarations of third-party types (`RtMidiIn`) must be global, and calls like `::AppendMenu` must be `ui::AppendMenu`.
- **`CTrackerSoundFile`** (`openmpt_ext/sndlib`) derives from `CSoundFile` and holds the tracker-only state (mod doc pointer, note names, channel colors, MIDI mapper, sample paths, row/order locks, mute toggles, DSP effects, metronome). Every `CSoundFile` the GUI creates must be a `CTrackerSoundFile`; use `TrackerSoundFile(sndFile)` to downcast engine-provided `CSoundFile&` (checked in debug builds). `CSoundFile` has no virtuals, so its "overrides" only work when called through `CTrackerSoundFile`.
- **Playback goes through `CTrackerSoundFile::Render()`**, not `Read()`. It renders up to each tick boundary (reading `PlayState::m_nBufferCount` through a pointer-to-protected-member) and applies upstream's row transition rules (row/order lock, step, queued transitions, mute toggles, metronome trigger) by adjusting `m_nNextRow`/`m_nNextOrder` before the next tick. It also applies the DSP effects and metronome to the output.
- **Locking.** libopenmpt's `CriticalSection` is a no-op. GUI code uses `TrackerCriticalSection`. Engine functions that lock internally in the tracker build (sample replace/resize/convert, `DestroyInstrument`, plugin creation/destruction, ...) must be called with the lock held: `CallLocked([&] { return ...; })` or the locking overrides on `CTrackerSoundFile`. Sound devices like PulseAudio Simple hold the lock across their blocking writes, so `CMainFrame::SoundCallbackLock` calls `Tracker::YieldToLockWaiters()` first; without it the unfair mutex starves the GUI thread.
- **Plugins.** The tracker-only `IMixPlugin` interface (parameter names, editors, automation, presets) is `PluginUi` (`src/mptrack/PluginUi.h`), backed by `openmpt_ext/plugins/PluginInfo` for the built-in effects. Each `CTrackerSoundFile` has its own `CVstPluginManager`; `PluginUi::RegisterTrackerPlugins` adds MIDI Input/Output to it. Close editors with `PluginUi::DestroyPlugin` / `CloseAllEditors` before plugins are released.
- **Declared but undefined members.** libopenmpt declares some tracker-only members without defining them (`SaveSFZInstrument`, `CVstPluginManager::AddPlugin/RemovePlugin/OnIdle`, `VSTPluginLib::WriteToCache`). Those are defined in `src/openmpt_ext`. Members that libopenmpt does define as stubs (`ReadMID`, `ReadSFZInstrument`, `ReadFLACSample`) cannot be redefined.

## Suspected regressions from linking libopenmpt

Known or suspected behaviour differences compared with the in-place tracker build. Most are unverified; check here first when something misbehaves.

Playback:
- Engine semantics compiled differently in library mode: XM `F00` (speed 0) handling, MT2 7-bit global volume, AMS envelope ranges, the "reset channels on loop" pattern setting (library always emulates a pattern break), automatic XM smooth ramping on load, and the sync-mute setting inside the engine (`CSoundFile::GetChannelMuteFlag` always returns `CHN_MUTE`).
- Row/order lock, step-play, queued pattern transitions and transition solo/unmute are emulated at tick boundaries. Step-play pauses after the row's last tick instead of while processing it. All of this relies on `ReadNote()` resetting `m_nBufferCount` to `m_nSamplesPerTick`.
- Upstream's end-of-song recheck after edits (`Sndmix.cpp`, "visited rows vector might have been screwed up") is not emulated.
- DSP effects (surround, mega bass, EQ, AGC, bitcrush) run after `Read()`, so VU meters show pre-DSP levels. The metronome uses a simple nearest-sample mixer, is not shown on VU meters, and uses the time signature of the current row when deciding measure/beat clicks.
- `m_SamplePlayLengths` is never filled, so the sample trimmer (finding unused sample data) does not work.
- Engine-to-GUI notifications are gone: dry/wet changes by MIDI macros (`m_pluginDryWetRatioChanged`), plugin MIDI output recording (`m_recordMIDIOut`), view updates on plugin bypass changes. Plugin editors are refreshed from the idle loop instead.
- Time stretch / pitch shift holds the audio lock for the whole operation, so audio stalls while it runs.

Files:
- Not loaded or saved: MIDI mappings (`MIMA` chunk), channel colors (`CCOL` chunk), the current session's edit history entry and creation time.
- Channel mute status is always saved (the "save channel mute status" setting is ignored); IT sample compression is never used.
- External samples are read back by re-parsing MPTM sample headers, but saving embeds them into the file. `.itp` projects cannot be loaded.
- MIDI (`.mid`/`.rmi`/`.smf`), WAV and UAX files cannot be opened as modules. SFZ instruments cannot be imported (export works). FLAC files load as samples but not as instruments. Opus samples are not supported; MP3/Vorbis use libopenmpt's minimp3/stb_vorbis.
- Upstream's tracker-only load messages (local tuning migration, missing plugin summary) are not emitted.

UI and other:
- Component manager removed (About dialog says all components are built in); `-noassembly` and CPU feature selection have no effect.
- I3DL2Reverb presets are applied by setting parameters; the selected preset is not stored in the module.
- Plugin tags are kept per plugin library in memory and the settings file; vendor names are derived (built-in non-DMO plugins are "OpenMPT Project").
- Debug log facility settings (file/debugger/console) are inactive; libopenmpt logs to stderr and lines before the settings are read always appear.
- Windows-only features remain removed: VST hosting, Media Foundation, Wine integration, driver crash masking.
- Windows and macOS builds are untested. The Win32 code paths in `AtomicFileExt.cpp` and `FileSystemExt.cpp` have never been compiled; libopenmpt on Windows needs MSYS2 (rsync, make, MinGW); macOS has no sound device backend configured.
