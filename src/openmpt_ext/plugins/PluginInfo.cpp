/*
 * PluginInfo.cpp
 * --------------
 * Purpose: Parameter and program names of the built-in plugins, which upstream only compiles into the tracker build.
 * Notes  : Ported from the tracker-only parts of soundlib/plugins. The plugin classes are final and keep their
 *          parameter enums protected, so the parameter order is repeated here.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "PluginInfo.h"

#include "../soundlib/plugins/DigiBoosterEcho.h"
#include "../soundlib/plugins/PlugInterface.h"
#include "../soundlib/plugins/SymMODEcho.h"
#include "../soundlib/plugins/dmo/Chorus.h"
#include "../soundlib/plugins/dmo/Compressor.h"
#include "../soundlib/plugins/dmo/Distortion.h"
#include "../soundlib/plugins/dmo/Echo.h"
#include "../soundlib/plugins/dmo/Flanger.h"
#include "../soundlib/plugins/dmo/Gargle.h"
#include "../soundlib/plugins/dmo/I3DL2Reverb.h"
#include "../soundlib/plugins/dmo/ParamEq.h"
#include "../soundlib/plugins/dmo/WavesReverb.h"
#include "../sounddsp/Reverb.h"

#include <cmath>

OPENMPT_NAMESPACE_BEGIN

namespace
{

enum ChorusParam : PlugParamIndex { kChorusWetDryMix, kChorusDepth, kChorusFrequency, kChorusWaveShape, kChorusPhase, kChorusFeedback, kChorusDelay };
enum FlangerParam : PlugParamIndex { kFlangerWetDryMix, kFlangerWaveShape, kFlangerFrequency, kFlangerDepth, kFlangerPhase, kFlangerFeedback, kFlangerDelay };
enum CompressorParam : PlugParamIndex { kCompGain, kCompAttack, kCompRelease, kCompThreshold, kCompRatio, kCompPredelay };
enum DistortionParam : PlugParamIndex { kDistGain, kDistEdge, kDistPreLowpassCutoff, kDistPostEQCenterFrequency, kDistPostEQBandwidth };
enum EchoParam : PlugParamIndex { kEchoWetDry, kEchoFeedback, kEchoLeftDelay, kEchoRightDelay, kEchoPanDelay };
enum GargleParam : PlugParamIndex { kGargleRate, kGargleWaveShape };
enum I3DL2Param : PlugParamIndex
{
	kI3DL2ReverbRoom, kI3DL2ReverbRoomHF, kI3DL2ReverbRoomRolloffFactor, kI3DL2ReverbDecayTime, kI3DL2ReverbDecayHFRatio,
	kI3DL2ReverbReflections, kI3DL2ReverbReflectionsDelay, kI3DL2ReverbReverb, kI3DL2ReverbReverbDelay, kI3DL2ReverbDiffusion,
	kI3DL2ReverbDensity, kI3DL2ReverbHFReference, kI3DL2ReverbQuality
};
enum ParamEqParam : PlugParamIndex { kEqCenter, kEqBandwidth, kEqGain };
enum WavesReverbParam : PlugParamIndex { kRvbInGain, kRvbReverbMix, kRvbReverbTime, kRvbHighFreqRTRatio };
enum DBMEchoParam : PlugParamIndex { kDBMEchoDelay, kDBMEchoFeedback, kDBMEchoMix, kDBMEchoCross };
enum SymMODEchoParam : PlugParamIndex { kSymEchoType, kSymEchoDelay, kSymEchoFeedback };

const mpt::ustring kDegreeSign = mpt::ToUnicode(mpt::Charset::UTF8, "\xC2\xB0");

mpt::ustring FormatFixed(float value)
{
	return mpt::ufmt::fix(value, 2);
}

uint8 FindChunkParam(IMixPlugin &plugin, PlugParamIndex paramIndex, float scale)
{
	return mpt::saturate_round<uint8>(plugin.GetParameter(paramIndex) * scale);
}

std::optional<mpt::ustring> FindPhaseDisplay(float value)
{
	switch(mpt::saturate_round<uint32>(value * 4.0f))
	{
	case 0: return U_("-180");
	case 1: return U_("-90");
	case 2: return U_("0");
	case 3: return U_("90");
	case 4: return U_("180");
	}
	return std::nullopt;
}

struct ChorusLikeLayout
{
	PlugParamIndex wetDryMix, depth, frequency, waveShape, phase, feedback, delay;
	float delayScale;
};

constexpr ChorusLikeLayout kChorusLayout{kChorusWetDryMix, kChorusDepth, kChorusFrequency, kChorusWaveShape, kChorusPhase, kChorusFeedback, kChorusDelay, 20.0f};
constexpr ChorusLikeLayout kFlangerLayout{kFlangerWetDryMix, kFlangerDepth, kFlangerFrequency, kFlangerWaveShape, kFlangerPhase, kFlangerFeedback, kFlangerDelay, 4.0f};

const ChorusLikeLayout *FindChorusLayout(const IMixPlugin &plugin)
{
	if(dynamic_cast<const DMO::Flanger *>(&plugin))
		return &kFlangerLayout;
	if(dynamic_cast<const DMO::Chorus *>(&plugin))
		return &kChorusLayout;
	return nullptr;
}

std::optional<mpt::ustring> FindChorusParamName(const ChorusLikeLayout &layout, PlugParamIndex paramIndex)
{
	if(paramIndex == layout.wetDryMix) return U_("WetDryMix");
	if(paramIndex == layout.depth) return U_("Depth");
	if(paramIndex == layout.frequency) return U_("Frequency");
	if(paramIndex == layout.waveShape) return U_("WaveShape");
	if(paramIndex == layout.phase) return U_("Phase");
	if(paramIndex == layout.feedback) return U_("Feedback");
	if(paramIndex == layout.delay) return U_("Delay");
	return mpt::ustring();
}

std::optional<mpt::ustring> FindChorusParamLabel(const ChorusLikeLayout &layout, PlugParamIndex paramIndex)
{
	if(paramIndex == layout.wetDryMix || paramIndex == layout.depth || paramIndex == layout.feedback) return U_("%");
	if(paramIndex == layout.frequency) return U_("Hz");
	if(paramIndex == layout.phase) return kDegreeSign;
	if(paramIndex == layout.delay) return U_("ms");
	return mpt::ustring();
}

std::optional<mpt::ustring> FindChorusParamDisplay(const ChorusLikeLayout &layout, IMixPlugin &plugin, PlugParamIndex paramIndex)
{
	const float value = plugin.GetParameter(paramIndex);
	if(paramIndex == layout.wetDryMix || paramIndex == layout.depth) return FormatFixed(value * 100.0f);
	if(paramIndex == layout.frequency) return FormatFixed(value * 10.0f);
	if(paramIndex == layout.waveShape) return (value < 1) ? U_("Square") : U_("Sine");
	if(paramIndex == layout.phase) return FindPhaseDisplay(value);
	if(paramIndex == layout.feedback) return FormatFixed(-99.0f + value * 198.0f);
	if(paramIndex == layout.delay) return FormatFixed(value * layout.delayScale);
	return FormatFixed(value);
}

}  // namespace


namespace PluginInfo
{

std::optional<mpt::ustring> FindDefaultEffectName(const IMixPlugin &plugin)
{
	if(dynamic_cast<const DMO::Flanger *>(&plugin)) return U_("Flanger");
	if(dynamic_cast<const DMO::Chorus *>(&plugin)) return U_("Chorus");
	if(dynamic_cast<const DMO::Compressor *>(&plugin)) return U_("Compressor");
	if(dynamic_cast<const DMO::Distortion *>(&plugin)) return U_("Distortion");
	if(dynamic_cast<const DMO::Echo *>(&plugin)) return U_("Echo");
	if(dynamic_cast<const DMO::Gargle *>(&plugin)) return U_("Gargle");
	if(dynamic_cast<const DMO::I3DL2Reverb *>(&plugin)) return U_("I3DL2Reverb");
	if(dynamic_cast<const DMO::ParamEq *>(&plugin)) return U_("ParamEq");
	if(dynamic_cast<const DMO::WavesReverb *>(&plugin)) return U_("WavesReverb");
	if(dynamic_cast<const DigiBoosterEcho *>(&plugin)) return U_("Echo");
	if(dynamic_cast<const SymMODEcho *>(&plugin)) return U_("Echo");
	return std::nullopt;
}


std::optional<mpt::ustring> FindParamName(IMixPlugin &plugin, PlugParamIndex paramIndex)
{
	if(const auto *layout = FindChorusLayout(plugin))
		return FindChorusParamName(*layout, paramIndex);
	if(dynamic_cast<const DMO::Compressor *>(&plugin))
	{
		switch(paramIndex)
		{
		case kCompGain: return U_("Gain");
		case kCompAttack: return U_("Attack");
		case kCompRelease: return U_("Release");
		case kCompThreshold: return U_("Threshold");
		case kCompRatio: return U_("Ratio");
		case kCompPredelay: return U_("Predelay");
		}
		return mpt::ustring();
	}
	if(dynamic_cast<const DMO::Distortion *>(&plugin))
	{
		switch(paramIndex)
		{
		case kDistGain: return U_("Gain");
		case kDistEdge: return U_("Edge");
		case kDistPreLowpassCutoff: return U_("PreLowpassCutoff");
		case kDistPostEQCenterFrequency: return U_("PostEQCenterFrequency");
		case kDistPostEQBandwidth: return U_("PostEQBandwidth");
		}
		return mpt::ustring();
	}
	if(dynamic_cast<const DMO::Echo *>(&plugin))
	{
		switch(paramIndex)
		{
		case kEchoWetDry: return U_("WetDryMix");
		case kEchoFeedback: return U_("Feedback");
		case kEchoLeftDelay: return U_("LeftDelay");
		case kEchoRightDelay: return U_("RightDelay");
		case kEchoPanDelay: return U_("PanDelay");
		}
		return mpt::ustring();
	}
	if(dynamic_cast<const DMO::Gargle *>(&plugin))
	{
		switch(paramIndex)
		{
		case kGargleRate: return U_("Rate");
		case kGargleWaveShape: return U_("WaveShape");
		}
		return mpt::ustring();
	}
	if(dynamic_cast<const DMO::I3DL2Reverb *>(&plugin))
	{
		switch(paramIndex)
		{
		case kI3DL2ReverbRoom: return U_("Room");
		case kI3DL2ReverbRoomHF: return U_("RoomHF");
		case kI3DL2ReverbRoomRolloffFactor: return U_("RoomRolloffFactor");
		case kI3DL2ReverbDecayTime: return U_("DecayTime");
		case kI3DL2ReverbDecayHFRatio: return U_("DecayHFRatio");
		case kI3DL2ReverbReflections: return U_("Reflections");
		case kI3DL2ReverbReflectionsDelay: return U_("ReflectionsDelay");
		case kI3DL2ReverbReverb: return U_("Reverb");
		case kI3DL2ReverbReverbDelay: return U_("ReverbDelay");
		case kI3DL2ReverbDiffusion: return U_("Diffusion");
		case kI3DL2ReverbDensity: return U_("Density");
		case kI3DL2ReverbHFReference: return U_("HFRefrence");
		case kI3DL2ReverbQuality: return U_("Quality");
		}
		return mpt::ustring();
	}
	if(dynamic_cast<const DMO::ParamEq *>(&plugin))
	{
		switch(paramIndex)
		{
		case kEqCenter: return U_("Center");
		case kEqBandwidth: return U_("Bandwidth");
		case kEqGain: return U_("Gain");
		}
		return mpt::ustring();
	}
	if(dynamic_cast<const DMO::WavesReverb *>(&plugin))
	{
		switch(paramIndex)
		{
		case kRvbInGain: return U_("InGain");
		case kRvbReverbMix: return U_("ReverbMix");
		case kRvbReverbTime: return U_("ReverbTime");
		case kRvbHighFreqRTRatio: return U_("HighFreqRTRatio");
		}
		return mpt::ustring();
	}
	if(dynamic_cast<const DigiBoosterEcho *>(&plugin))
	{
		switch(paramIndex)
		{
		case kDBMEchoDelay: return U_("Delay");
		case kDBMEchoFeedback: return U_("Feedback");
		case kDBMEchoMix: return U_("Wet / Dry Ratio");
		case kDBMEchoCross: return U_("Cross Echo");
		}
		return mpt::ustring();
	}
	if(dynamic_cast<const SymMODEcho *>(&plugin))
	{
		switch(paramIndex)
		{
		case kSymEchoType: return U_("Type");
		case kSymEchoDelay: return U_("Delay");
		case kSymEchoFeedback: return U_("Feedback");
		}
		return mpt::ustring();
	}
	return std::nullopt;
}


std::optional<mpt::ustring> FindParamLabel(IMixPlugin &plugin, PlugParamIndex paramIndex)
{
	if(const auto *layout = FindChorusLayout(plugin))
		return FindChorusParamLabel(*layout, paramIndex);
	if(dynamic_cast<const DMO::Compressor *>(&plugin))
	{
		switch(paramIndex)
		{
		case kCompGain:
		case kCompThreshold:
			return U_("dB");
		case kCompAttack:
		case kCompRelease:
		case kCompPredelay:
			return U_("ms");
		case kCompRatio:
			return U_(": 1");
		}
		return mpt::ustring();
	}
	if(dynamic_cast<const DMO::Distortion *>(&plugin))
	{
		switch(paramIndex)
		{
		case kDistGain:
			return U_("dB");
		case kDistPreLowpassCutoff:
		case kDistPostEQCenterFrequency:
		case kDistPostEQBandwidth:
			return U_("Hz");
		}
		return mpt::ustring();
	}
	if(dynamic_cast<const DMO::Echo *>(&plugin))
	{
		switch(paramIndex)
		{
		case kEchoFeedback:
			return U_("%");
		case kEchoLeftDelay:
		case kEchoRightDelay:
			return U_("ms");
		}
		return mpt::ustring();
	}
	if(dynamic_cast<const DMO::Gargle *>(&plugin))
		return (paramIndex == kGargleRate) ? U_("Hz") : mpt::ustring();
	if(dynamic_cast<const DMO::I3DL2Reverb *>(&plugin))
	{
		switch(paramIndex)
		{
		case kI3DL2ReverbRoom:
		case kI3DL2ReverbRoomHF:
		case kI3DL2ReverbReflections:
		case kI3DL2ReverbReverb:
			return U_("dB");
		case kI3DL2ReverbDecayTime:
		case kI3DL2ReverbReflectionsDelay:
		case kI3DL2ReverbReverbDelay:
			return U_("s");
		case kI3DL2ReverbDiffusion:
		case kI3DL2ReverbDensity:
			return U_("%");
		case kI3DL2ReverbHFReference:
			return U_("Hz");
		}
		return mpt::ustring();
	}
	if(dynamic_cast<const DMO::ParamEq *>(&plugin))
	{
		switch(paramIndex)
		{
		case kEqCenter: return U_("Hz");
		case kEqBandwidth: return U_("Semitones");
		case kEqGain: return U_("dB");
		}
		return mpt::ustring();
	}
	if(dynamic_cast<const DMO::WavesReverb *>(&plugin))
	{
		switch(paramIndex)
		{
		case kRvbInGain:
		case kRvbReverbMix:
			return U_("dB");
		case kRvbReverbTime:
			return U_("ms");
		}
		return mpt::ustring();
	}
	if(dynamic_cast<const DigiBoosterEcho *>(&plugin))
		return (paramIndex == kDBMEchoDelay) ? U_("ms") : mpt::ustring();
	if(dynamic_cast<const SymMODEcho *>(&plugin))
	{
		if(paramIndex == kSymEchoDelay)
			return U_("Ticks");
		if(paramIndex == kSymEchoFeedback)
			return U_("%");
		return mpt::ustring();
	}
	return std::nullopt;
}


std::optional<mpt::ustring> FindParamDisplay(IMixPlugin &plugin, PlugParamIndex paramIndex)
{
	if(const auto *layout = FindChorusLayout(plugin))
		return FindChorusParamDisplay(*layout, plugin, paramIndex);

	const float value = plugin.GetParameter(paramIndex);
	if(dynamic_cast<const DMO::Compressor *>(&plugin))
	{
		switch(paramIndex)
		{
		case kCompGain: return FormatFixed(-60.0f + value * 120.0f);
		case kCompAttack: return FormatFixed(0.01f + value * 499.99f);
		case kCompRelease: return FormatFixed(50.0f + value * 2950.0f);
		case kCompThreshold: return FormatFixed(-60.0f + value * 60.0f);
		case kCompRatio: return FormatFixed(1.0f + value * 99.0f);
		case kCompPredelay: return FormatFixed(value * 4.0f);
		}
		return FormatFixed(value);
	}
	if(dynamic_cast<const DMO::Distortion *>(&plugin))
	{
		switch(paramIndex)
		{
		case kDistGain: return FormatFixed(-60.0f + value * 60.0f);
		case kDistEdge: return FormatFixed(value * 100.0f);
		case kDistPreLowpassCutoff:
		case kDistPostEQCenterFrequency:
		case kDistPostEQBandwidth:
			return FormatFixed(100.0f + value * 7900.0f);
		}
		return FormatFixed(value);
	}
	if(dynamic_cast<const DMO::Echo *>(&plugin))
	{
		switch(paramIndex)
		{
		case kEchoWetDry: return mpt::ufmt::fix(value * 100.0f, 1) + U_(" : ") + mpt::ufmt::fix(100.0f - value * 100.0f, 1);
		case kEchoFeedback: return FormatFixed(value * 100.0f);
		case kEchoLeftDelay:
		case kEchoRightDelay:
			return FormatFixed(1.0f + value * 1999.0f);
		case kEchoPanDelay: return (value <= 0.5) ? U_("No") : U_("Yes");
		}
		return mpt::ustring();
	}
	if(dynamic_cast<const DMO::Gargle *>(&plugin))
	{
		switch(paramIndex)
		{
		case kGargleRate: return mpt::ufmt::val(static_cast<uint32>(mpt::round(std::clamp(value, 0.0f, 1.0f) * 999.0f)) + 1);
		case kGargleWaveShape: return (value < 0.5) ? U_("Triangle") : U_("Square");
		}
		return mpt::ustring();
	}
	if(dynamic_cast<const DMO::I3DL2Reverb *>(&plugin))
	{
		static const mpt::ustring modes[] = {U_("LQ"), U_("LQ+"), U_("HQ"), U_("HQ+")};
		switch(paramIndex)
		{
		case kI3DL2ReverbRoom: return FormatFixed((-10000.0f + value * 10000.0f) * 0.01f);
		case kI3DL2ReverbRoomHF: return FormatFixed((-10000.0f + value * 10000.0f) * 0.01f);
		case kI3DL2ReverbRoomRolloffFactor: return FormatFixed(value * 10.0f);
		case kI3DL2ReverbDecayTime: return FormatFixed(0.1f + value * 19.9f);
		case kI3DL2ReverbDecayHFRatio: return FormatFixed(0.1f + value * 1.9f);
		case kI3DL2ReverbReflections: return FormatFixed((-10000.0f + value * 11000.0f) * 0.01f);
		case kI3DL2ReverbReflectionsDelay: return FormatFixed(value * 0.3f);
		case kI3DL2ReverbReverb: return FormatFixed((-10000.0f + value * 12000.0f) * 0.01f);
		case kI3DL2ReverbReverbDelay: return FormatFixed(value * 0.1f);
		case kI3DL2ReverbDiffusion: return FormatFixed(value * 100.0f);
		case kI3DL2ReverbDensity: return FormatFixed(value * 100.0f);
		case kI3DL2ReverbHFReference: return FormatFixed(20.0f + value * 19980.0f);
		case kI3DL2ReverbQuality: return modes[mpt::saturate_round<uint32>(value * 3.0f) % 4u];
		}
		return FormatFixed(value);
	}
	if(dynamic_cast<const DMO::ParamEq *>(&plugin))
	{
		switch(paramIndex)
		{
		case kEqCenter: return FormatFixed(80.0f + value * 15920.0f);
		case kEqBandwidth: return FormatFixed(1.0f + value * 35.0f);
		case kEqGain: return FormatFixed((value - 0.5f) * 30.0f);
		}
		return FormatFixed(0.0f);
	}
	if(dynamic_cast<const DMO::WavesReverb *>(&plugin))
	{
		switch(paramIndex)
		{
		case kRvbInGain:
		case kRvbReverbMix:
			return FormatFixed(-96.0f + value * 96.0f);
		case kRvbReverbTime: return FormatFixed(0.001f + value * 2999.999f);
		case kRvbHighFreqRTRatio: return FormatFixed(0.001f + value * 0.998f);
		}
		return FormatFixed(value);
	}
	if(dynamic_cast<const DigiBoosterEcho *>(&plugin))
	{
		if(paramIndex == kDBMEchoMix)
		{
			const int wetPercent = (FindChunkParam(plugin, kDBMEchoMix, 255.0f) * 100) / 255;
			return MPT_UFORMAT("{}% / {}%")(wetPercent, 100 - wetPercent);
		}
		if(paramIndex > kDBMEchoCross)
			return mpt::ustring();
		int chunkValue = FindChunkParam(plugin, paramIndex, 255.0f);
		if(paramIndex == kDBMEchoDelay)
		{
			if(chunkValue == 0)
				chunkValue = 167;
			chunkValue *= 2;
		}
		return mpt::ufmt::val(chunkValue);
	}
	if(dynamic_cast<const SymMODEcho *>(&plugin))
	{
		switch(paramIndex)
		{
		case kSymEchoType:
			switch(static_cast<SymMODEcho::DSPType>(FindChunkParam(plugin, kSymEchoType, 127.0f)))
			{
			case SymMODEcho::DSPType::Off: return U_("Off");
			case SymMODEcho::DSPType::Normal: return U_("Normal");
			case SymMODEcho::DSPType::Cross: return U_("Cross");
			case SymMODEcho::DSPType::Cross2: return U_("Cross 2");
			case SymMODEcho::DSPType::Center: return U_("Center");
			case SymMODEcho::DSPType::NumTypes: break;
			}
			return mpt::ustring();
		case kSymEchoDelay:
			return mpt::ufmt::val(FindChunkParam(plugin, kSymEchoDelay, 127.0f));
		case kSymEchoFeedback:
		{
			const bool isCross2 = static_cast<SymMODEcho::DSPType>(FindChunkParam(plugin, kSymEchoType, 127.0f)) == SymMODEcho::DSPType::Cross2;
			const float feedbackParam = static_cast<float>(FindChunkParam(plugin, kSymEchoFeedback, 127.0f));
			const float feedback = isCross2 ? 1.0f - std::pow(2.0f, -(feedbackParam + 1.0f)) : std::pow(2.0f, -feedbackParam);
			return mpt::ufmt::flt(feedback * 100.0f, 4);
		}
		}
		return mpt::ustring();
	}
	return std::nullopt;
}


std::optional<std::pair<PlugParamValue, PlugParamValue>> FindParamUIRange(IMixPlugin &plugin, PlugParamIndex paramIndex)
{
	if(dynamic_cast<const SymMODEcho *>(&plugin) && paramIndex == kSymEchoType)
		return std::make_pair(0.0f, (static_cast<uint8>(SymMODEcho::DSPType::NumTypes) - 1) / 127.0f);
	return std::nullopt;
}


int32 FindNumPrograms(const IMixPlugin &plugin)
{
	if(dynamic_cast<const DMO::I3DL2Reverb *>(&plugin))
		return NUM_REVERBTYPES;
	return 0;
}


mpt::ustring FindProgramName(const IMixPlugin &plugin, int32 programIndex)
{
	if(dynamic_cast<const DMO::I3DL2Reverb *>(&plugin) && programIndex >= 0)
		return GetReverbPresetName(static_cast<uint32>(programIndex));
	return mpt::ustring();
}


bool SetProgram(IMixPlugin &plugin, int32 programIndex)
{
	if(!dynamic_cast<const DMO::I3DL2Reverb *>(&plugin) || programIndex < 0 || programIndex >= static_cast<int32>(NUM_REVERBTYPES))
		return false;

	const SNDMIX_REVERB_PROPERTIES &preset = *GetReverbPreset(static_cast<uint32>(programIndex));
	plugin.SetParameter(kI3DL2ReverbRoom, (preset.lRoom + 10000) / 10000.0f);
	plugin.SetParameter(kI3DL2ReverbRoomHF, (preset.lRoomHF + 10000) / 10000.0f);
	plugin.SetParameter(kI3DL2ReverbRoomRolloffFactor, 0.0f);
	plugin.SetParameter(kI3DL2ReverbDecayTime, (preset.flDecayTime - 0.1f) / 19.9f);
	plugin.SetParameter(kI3DL2ReverbDecayHFRatio, (preset.flDecayHFRatio - 0.1f) / 1.9f);
	plugin.SetParameter(kI3DL2ReverbReflections, (preset.lReflections + 10000) / 11000.0f);
	plugin.SetParameter(kI3DL2ReverbReflectionsDelay, preset.flReflectionsDelay / 0.3f);
	plugin.SetParameter(kI3DL2ReverbReverb, (preset.lReverb + 10000) / 12000.0f);
	plugin.SetParameter(kI3DL2ReverbReverbDelay, preset.flReverbDelay / 0.1f);
	plugin.SetParameter(kI3DL2ReverbDiffusion, preset.flDiffusion / 100.0f);
	plugin.SetParameter(kI3DL2ReverbDensity, preset.flDensity / 100.0f);
	plugin.SetParameter(kI3DL2ReverbHFReference, (5000.0f - 20.0f) / 19980.0f);
	return true;
}

}  // namespace PluginInfo

OPENMPT_NAMESPACE_END
