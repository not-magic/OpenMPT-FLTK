/*
 * MIDIMacrosExt.h
 * ---------------
 * Purpose: MIDI macro descriptions and parsing helpers that upstream only compiles into the tracker build.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../soundlib/MIDIMacros.h"

OPENMPT_NAMESPACE_BEGIN

class IMixPlugin;

// Macro description including plugin parameter / MIDI CC information
mpt::ustring GetParameteredMacroName(const MIDIMacroConfig &config, uint32 macroIndex, IMixPlugin *plugin = nullptr);
mpt::ustring GetParameteredMacroName(const MIDIMacroConfig &config, ParameteredMacro macroType);
mpt::ustring GetFixedMacroName(const MIDIMacroConfig &config, FixedMacro macroType);
PlugParamIndex MacroToPlugParam(const MIDIMacroConfig &config, uint32 macroIndex);
int MacroToMidiCC(const MIDIMacroConfig &config, uint32 macroIndex);
int FindMacroForParam(const MIDIMacroConfig &config, PlugParamIndex paramIndex);

OPENMPT_NAMESPACE_END
