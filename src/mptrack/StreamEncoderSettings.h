// FLTK port of openmpt/mptrack/StreamEncoderSettings.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "openmpt/streamencoder/StreamEncoder.hpp"
#include "Settings.h"


OPENMPT_NAMESPACE_BEGIN



template<> inline SettingValue ToSettingValue(const Encoder::Mode &val)
{
	switch(val)
	{
	case Encoder::ModeCBR: return SettingValue(U_("CBR"), "Encoder::Mode"); break;
	case Encoder::ModeABR: return SettingValue(U_("ABR"), "Encoder::Mode"); break;
	case Encoder::ModeVBR: return SettingValue(U_("VBR"), "Encoder::Mode"); break;
	case Encoder::ModeQuality: return SettingValue(U_("Quality"), "Encoder::Mode"); break;
	case Encoder::ModeLossless: return SettingValue(U_("Lossless"), "Encoder::Mode"); break;
	default: return SettingValue(U_("Invalid"), "Encoder::Mode"); break;
	}
}
template<> inline Encoder::Mode FromSettingValue(const SettingValue &val)
{
	MPT_ASSERT(val.GetTypeTag() == "Encoder::Mode");
	if(val.as<mpt::ustring>() == UL_("")) { return Encoder::ModeInvalid; }
	else if(val.as<mpt::ustring>() == UL_("CBR")) { return Encoder::ModeCBR; }
	else if(val.as<mpt::ustring>() == UL_("ABR")) { return Encoder::ModeABR; }
	else if(val.as<mpt::ustring>() == UL_("VBR")) { return Encoder::ModeVBR; }
	else if(val.as<mpt::ustring>() == UL_("Quality")) { return Encoder::ModeQuality; }
	else if(val.as<mpt::ustring>() == UL_("Lossless")) { return Encoder::ModeLossless; }
	else { return Encoder::ModeInvalid; }
}


template<> inline SettingValue ToSettingValue(const Encoder::Format &val)
{
	return SettingValue(val.AsInt(), "Encoder::Format");
}
template<> inline Encoder::Format FromSettingValue(const SettingValue &val)
{
	MPT_ASSERT(val.GetTypeTag() == "Encoder::Format");
	return Encoder::Format::FromInt(val.as<int32>());
}


struct StoredTags
{
	Setting<mpt::ustring> artist;
	Setting<mpt::ustring> album;
	Setting<mpt::ustring> trackno;
	Setting<mpt::ustring> year;
	Setting<mpt::ustring> url;

	Setting<mpt::ustring> genre;

	StoredTags(SettingsContainer &conf);

};


struct EncoderSettingsConf
{

	Setting<bool> Cues;
	Setting<bool> Tags;

	Setting<uint32> Samplerate;
	Setting<uint16> Channels;

	Setting<Encoder::Mode> Mode;
	Setting<int> Bitrate;
	Setting<float> Quality;
	Setting<Encoder::Format> Format2;
	Setting<int> Dither;

	EncoderSettingsConf(SettingsContainer &conf, const mpt::ustring &encoderName, bool cues, bool tags, uint32 samplerate, uint16 channels, Encoder::Mode mode, int bitrate, float quality, Encoder::Format format, int dither);

	explicit operator Encoder::Settings() const;

};


struct StreamEncoderSettingsConf
{
	Setting<int32> FLACCompressionLevel;
	Setting<bool> FLACMultithreading;
	Setting<uint32> AUPaddingAlignHint;
	Setting<uint32> MP3ID3v2MinPadding;
	Setting<uint32> MP3ID3v2PaddingAlignHint;
	Setting<bool> MP3ID3v2WriteReplayGainTXXX;
	Setting<int32> MP3LameQuality;
	Setting<bool> MP3LameID3v2UseLame;
	Setting<bool> MP3LameCalculateReplayGain;
	Setting<bool> MP3LameCalculatePeakSample;
	Setting<int32> OpusComplexity;
	StreamEncoderSettingsConf(SettingsContainer &conf, const mpt::ustring &section);
	explicit operator Encoder::StreamSettings() const;
};


OPENMPT_NAMESPACE_END
