# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A git mirror of the OpenMPT/libopenmpt Subversion trunk (upstream is SVN; GitHub is downstream). It contains:
- **OpenMPT**: the Windows/MFC tracker GUI (`mptrack/`, `pluginBridge/`, `sounddsp/`, `tracklib/`). Built only with Visual Studio (`build/vs20*/OpenMPT.sln`); not buildable on Linux.
- **libopenmpt / openmpt123**: the portable playback library and CLI player, built with the root `Makefile` (or autotools / CMake-less variants under `build/`).

The playback engine (`soundlib/`) is shared by both. This working copy is on Linux, so day-to-day builds use the libopenmpt Makefile.

Upstream's `doc/contributing.md` states they do not accept contributions developed or assisted by LLMs; keep that in mind before proposing upstreaming work.

## Build and test (libopenmpt, Linux)

```
make -j$(nproc)                 # libopenmpt, openmpt123, examples, test binary
make STRICT=1 AUTO_DEPS=1       # as CI does: warnings are errors, use system deps via pkg-config
make check                      # builds and runs bin/libopenmpt_test
make DEBUG=1 CHECKED=1          # debug build with runtime assertions
make CONFIG=clang               # or gcc|mingw-w64|emscripten|djgpp
make clean
```

- Build options (`NO_ZLIB=1`, `LOCAL_MPG123=1`, `OPTIMIZE=`, `CHECKED_ADDRESS=1`, `ONLY_TEST=1`, ...) are documented at the top of `Makefile`; per-config defaults live in `build/make/`.
- The test suite is a single binary (`libopenmpt_test`), sources in `test/` (OpenMPT/mpt tests, `test.cpp`) and `libopenmpt/libopenmpt_test/`; there is no per-test filter. Run it from `bin/` (the `make check` target handles the cwd) because it loads `test/*.mod|xm|it|...` fixtures.
- CI: `.github/workflows/*-Makefile.yml` and `*-Autotools.yml` are the reference invocations per platform.

## Architecture

- `soundlib/` — the engine: `CSoundFile` (`Sndfile.*`), one `Load_<fmt>.cpp` per format (each exposes `ProbeFileHeader<Fmt>`/`Read<Fmt>`, registered via `MPT_DECLARE_FORMAT` in `Sndfile.cpp`), `Snd_fx.cpp` (effect processing), `Fastmix.cpp` + mixer headers, `Tables.cpp`, `plugins/` (VST/DMO host). Shared with OpenMPT, so code here uses `#ifdef MODPLUG_TRACKER` to separate tracker-only features from the library build.
- `libopenmpt/` — public C (`libopenmpt.h`) and C++ (`libopenmpt.hpp`) API, `libopenmpt_impl.cpp` bridging to `soundlib`, extension interface (`libopenmpt_ext*`), plugins for Winamp/XMPlay, language bindings.
- `src/mpt/` — the standalone, header-mostly "mpt" support library (strings, IO, endian, format, random, UUID, ...) with its own tests. `src/openmpt/` holds OpenMPT-specific base/sound-device/sound-file code.
- `common/` — older shared utilities (`BuildSettings.h`, `FileReader.h`, `mptStringBuffer`, serialization, logging); `BuildSettings.h` selects feature macros per target.
- `openmpt123/` — CLI player; `unarchiver/`, `misc/` — support code for the tracker.
- `build/` — all build systems: `make/` (Makefile configs), `autotools/`, `vs*/` + `premake/` (VS projects are generated; use `build/regenerate_vs_projects.sh`, don't hand-edit), `download_externals.*` (fetches third-party deps into `include/`), `pch/`.
- `include/` and `contrib/` are third-party / non-integral assets with their own licenses.

## Conventions (from `doc/openmpt_styleguide.md`, `doc/libopenmpt_styleguide.md`)

- Two styles: the OpenMPT style (everything except `libopenmpt/` and `openmpt123/`) uses Allman braces, CamelCase names, tabs; `libopenmpt/` and `openmpt123/` use the libopenmpt style (K&R braces, spaces inside parentheses `foo( x )`, snake_case, tabs). Match the directory you are editing.
- Use the custom index types (`SAMPLEINDEX`, `ORDERINDEX`, ...) in soundlib code.
- When changing playback behaviour, gate it with `CSoundFile::IsCompatibleMode()` so old modules still render the same.
- Commit messages follow the `[Tag] area: message.` style seen in `git log` (`[Fix]`, `[Imp]`, `[Ref]`, `[Var]`, ...).

## Linux FLTK port of mptrack/

`mptrack/` has been converted in place from MFC to FLTK (the pristine MFC sources are in `../openmpt` for reference).

- Toolkit: `mptrack/ui/` (namespace `ui`, `Wnd`/`WndT<FlBase>`, message maps, `Painter` as the GDI replacement, controls, dialogs, `ListCtrl`/`TreeCtrl`/`TabCtrl`/`ToolBar`/`StatusBar`, framework classes). Windows resources are generated from `mptrack.rc` by `build/tools/rc2cpp.py` into `mptrack/res/ResourceData.cpp`.
- FLTK is the `fltk/` submodule, built in `build/fltk-linux` (`fltk-config` must be executable).
- Build: `cd build/premake-linux && ../../include/premake/bin/release/premake5 --file=premake.lua ninja && ninja -C ../ninja-linux` produces `bin/Debug/ninja-linux/OpenMPT`. The generated ninja files do not track header dependencies; run `ninja -t clean` after header edits.
- The root `Makefile` only builds libopenmpt/openmpt123 and must keep working (`make -j31 check`); shared dirs keep their library branches.
- Windows-only features are removed: VST hosting, DMO host, crash handler, IPC, Wine integration, update check, HTML help (opens the online manual).
