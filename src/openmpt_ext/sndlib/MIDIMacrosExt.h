// Tracker-only parts of openmpt/soundlib/MIDIMacros.h as free functions.

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
