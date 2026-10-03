// Tracker-only parts of openmpt/soundlib/MIDIMacros.cpp as free functions.

#include "stdafx.h"
#include "MIDIMacrosExt.h"

#include "PluginUi.h"

OPENMPT_NAMESPACE_BEGIN

namespace
{

int FindHexDigitValue(char digit)
{
	if(digit >= '0' && digit <= '9')
		return digit - '0';
	if(digit >= 'A' && digit <= 'F')
		return digit - 'A' + 0x0A;
	return 0;
}

// Reads the two hex digits at offset of the normalized macro string
int FindMacroByte(const std::string &macro, std::size_t offset)
{
	if(macro.size() < offset + 2)
		return 0;
	return (FindHexDigitValue(macro[offset]) << 4) + FindHexDigitValue(macro[offset + 1]);
}

}  // namespace


mpt::ustring GetParameteredMacroName(const MIDIMacroConfig &config, uint32 macroIndex, IMixPlugin *plugin)
{
	const ParameteredMacro macroType = config.GetParameteredMacroType(macroIndex);
	switch(macroType)
	{
	case kSFxPlugParam:
	{
		const PlugParamIndex paramIndex = MacroToPlugParam(config, macroIndex);
		mpt::ustring formattedName = MPT_UFORMAT("Param {}")(paramIndex);
		if(plugin == nullptr)
			return formattedName + U_(" (N/A)");
		const mpt::ustring paramName = PluginUi(*plugin).GetParamName(paramIndex);
		if(!paramName.empty())
			formattedName += U_(" (") + paramName + U_(")");
		return formattedName;
	}
	case kSFxCC:
		return MPT_UFORMAT("MIDI CC {}")(MacroToMidiCC(config, macroIndex));
	default:
		return GetParameteredMacroName(config, macroType);
	}
}


mpt::ustring GetParameteredMacroName(const MIDIMacroConfig &, ParameteredMacro macroType)
{
	switch(macroType)
	{
	case kSFxUnused:     return U_("Unused");
	case kSFxCutoff:     return U_("Set Filter Cutoff");
	case kSFxReso:       return U_("Set Filter Resonance");
	case kSFxFltMode:    return U_("Set Filter Mode");
	case kSFxDryWet:     return U_("Set Plugin Dry/Wet Ratio");
	case kSFxPlugParam:  return U_("Control Plugin Parameter...");
	case kSFxCC:         return U_("MIDI CC...");
	case kSFxChannelAT:  return U_("Channel Aftertouch");
	case kSFxPolyAT:     return U_("Polyphonic Aftertouch");
	case kSFxPitch:      return U_("Pitch Bend");
	case kSFxProgChange: return U_("MIDI Program Change");
	case kSFxCustom:
	default:             return U_("Custom");
	}
}


mpt::ustring GetFixedMacroName(const MIDIMacroConfig &, FixedMacro macroType)
{
	switch(macroType)
	{
	case kZxxUnused:      return U_("Unused");
	case kZxxReso4Bit:    return U_("Z80 - Z8F controls Resonant Filter Resonance");
	case kZxxReso7Bit:    return U_("Z80 - ZFF controls Resonant Filter Resonance");
	case kZxxCutoff:      return U_("Z80 - ZFF controls Resonant Filter Cutoff");
	case kZxxFltMode:     return U_("Z80 - ZFF controls Resonant Filter Mode");
	case kZxxResoFltMode: return U_("Z80 - Z9F controls Resonance + Filter Mode");
	case kZxxChannelAT:   return U_("Z80 - ZFF controls Channel Aftertouch");
	case kZxxPolyAT:      return U_("Z80 - ZFF controls Polyphonic Aftertouch");
	case kZxxPitch:       return U_("Z80 - ZFF controls Pitch Bend");
	case kZxxProgChange:  return U_("Z80 - ZFF controls MIDI Program Change");
	case kZxxCustom:
	default:              return U_("Custom");
	}
}


PlugParamIndex MacroToPlugParam(const MIDIMacroConfig &config, uint32 macroIndex)
{
	const std::string macro = config.SFx[macroIndex].NormalizedString();
	const PlugParamIndex codeValue = FindMacroByte(macro, 4);
	if(macro.size() >= 4 && macro[3] == '0')
		return codeValue - 128;
	return codeValue + 128;
}


int MacroToMidiCC(const MIDIMacroConfig &config, uint32 macroIndex)
{
	return FindMacroByte(config.SFx[macroIndex].NormalizedString(), 2);
}


int FindMacroForParam(const MIDIMacroConfig &config, PlugParamIndex paramIndex)
{
	for(int macroIndex = 0; macroIndex < kSFxMacros; ++macroIndex)
	{
		if(config.GetParameteredMacroType(macroIndex) == kSFxPlugParam && MacroToPlugParam(config, macroIndex) == paramIndex)
			return macroIndex;
	}
	return -1;
}

OPENMPT_NAMESPACE_END
