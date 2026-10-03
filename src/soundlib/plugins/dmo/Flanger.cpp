/*
 * Flanger.cpp
 * -----------
 * Purpose: Implementation of the DMO Flanger DSP (for non-Windows platforms)
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"

#include "Flanger.h"
#include "../../Sndfile.h"

OPENMPT_NAMESPACE_BEGIN


namespace DMO
{

// cppcheck-suppress duplInheritedMember
IMixPlugin* Flanger::Create(VSTPluginLib &factory, CSoundFile &sndFile, SNDMIXPLUGIN &mixStruct)
{
	return new (std::nothrow) Flanger(factory, sndFile, mixStruct, false);
}

IMixPlugin* Flanger::CreateLegacy(VSTPluginLib &factory, CSoundFile &sndFile, SNDMIXPLUGIN &mixStruct)
{
	return new(std::nothrow) Flanger(factory, sndFile, mixStruct, true);
}


Flanger::Flanger(VSTPluginLib &factory, CSoundFile &sndFile, SNDMIXPLUGIN &mixStruct, const bool legacy)
	: Chorus(factory, sndFile, mixStruct, !legacy)
{
	m_param[kFlangerWetDryMix] = 0.5f;
	m_param[kFlangerWaveShape] = 1.0f;
	m_param[kFlangerFrequency] = 0.025f;
	m_param[kFlangerDepth] = 1.0f;
	m_param[kFlangerPhase] = 0.5f;
	m_param[kFlangerFeedback] = (-50.0f + 99.0f) / 198.0f;
	m_param[kFlangerDelay] = 0.5f;

	// Already done in Chorus constructor
	//m_mixBuffer.Initialize(2, 2);
}


void Flanger::SetParameter(PlugParamIndex index, PlugParamValue value, PlayState *, CHANNELINDEX)
{
	if(index < kFlangerNumParameters)
	{
		value = mpt::safe_clamp(value, 0.0f, 1.0f);
		if(index == kFlangerWaveShape)
		{
			value = mpt::round(value);
			if(m_param[index] != value)
			{
				m_waveShapeMin = 0.0f;
				m_waveShapeMax = 0.5f + value * 0.5f;
			}
		} else if(index == kFlangerPhase)
		{
			value = mpt::round(value * 4.0f) / 4.0f;
		}
		m_param[index] = value;
		RecalculateChorusParams();
	}
}


#ifdef MODPLUG_TRACKER

mpt::ustring Flanger::GetParamName(PlugParamIndex param)
{
	switch(param)
	{
	case kFlangerWetDryMix: return U_("WetDryMix");
	case kFlangerWaveShape: return U_("WaveShape");
	case kFlangerFrequency: return U_("Frequency");
	case kFlangerDepth: return U_("Depth");
	case kFlangerPhase: return U_("Phase");
	case kFlangerFeedback: return U_("Feedback");
	case kFlangerDelay: return U_("Delay");
	}
	return mpt::ustring();
}


mpt::ustring Flanger::GetParamLabel(PlugParamIndex param)
{
	switch(param)
	{
	case kFlangerWetDryMix:
	case kFlangerDepth:
	case kFlangerFeedback:
		return U_("%");
	case kFlangerFrequency:
		return U_("Hz");
	case kFlangerPhase:
		return mpt::ToUnicode(mpt::Charset::UTF8, "\xC2\xB0");  // U+00B0 DEGREE SIGN
	case kFlangerDelay:
		return U_("ms");
	}
	return mpt::ustring();
}


mpt::ustring Flanger::GetParamDisplay(PlugParamIndex param)
{
	mpt::ustring s;
	float value = m_param[param];
	switch(param)
	{
	case kFlangerWetDryMix:
	case kFlangerDepth:
		value *= 100.0f;
		break;
	case kFlangerFrequency:
		value = FrequencyInHertz();
		break;
	case kFlangerWaveShape:
		return (value < 1) ? U_("Square") : U_("Sine");
		break;
	case kFlangerPhase:
		switch(Phase())
		{
		case 0: return U_("-180");
		case 1: return U_("-90");
		case 2: return U_("0");
		case 3: return U_("90");
		case 4: return U_("180");
		}
		break;
	case kFlangerFeedback:
		value = Feedback();
		break;
	case kFlangerDelay:
		value = Delay();
	}
	s = mpt::ufmt::fix(value, 2);
	return s;
}

#endif // MODPLUG_TRACKER

} // namespace DMO


OPENMPT_NAMESPACE_END
