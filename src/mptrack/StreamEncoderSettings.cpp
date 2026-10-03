/*
 * StreamEncoderSettings.cpp
 * -------------------------
 * Purpose: Exporting streamed music files.
 * Notes  : none
 * Authors: Joern Heusipp
 *          OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "stdafx.h"
#include "ui/Ui.h"

#include "StreamEncoderSettings.h"

#include "TrackerSettings.h"


OPENMPT_NAMESPACE_BEGIN


static mpt::ustring GetDefaultYear()
{
	const std::time_t now = std::time(nullptr);
	std::tm localTime{};
	localtime_r(&now, &localTime);
	return mpt::ufmt::dec(localTime.tm_year + 1900);
}


StoredTags::StoredTags(SettingsContainer &conf)
	: artist(conf, UL_("Export"), UL_("TagArtist"), TrackerSettings::Instance().defaultArtist)
	, album(conf, UL_("Export"), UL_("TagAlbum"), UL_(""))
	, trackno(conf, UL_("Export"), UL_("TagTrackNo"), UL_(""))
	, year(conf, UL_("Export"), UL_("TagYear"), GetDefaultYear())
	, url(conf, UL_("Export"), UL_("TagURL"), UL_(""))
	, genre(conf, UL_("Export"), UL_("TagGenre"), UL_(""))
{
	return;
}


EncoderSettingsConf::EncoderSettingsConf(SettingsContainer &conf, const mpt::ustring &encoderName, bool cues, bool tags, uint32 samplerate, uint16 channels, Encoder::Mode mode, int bitrate, float quality, Encoder::Format format, int dither)
	: Cues(conf, UL_("Export"), encoderName + UL_("_") + UL_("Cues"), cues)
	, Tags(conf, UL_("Export"), encoderName + UL_("_") + UL_("Tags"), tags)
	, Samplerate(conf, UL_("Export"), encoderName + UL_("_") + UL_("Samplerate"), samplerate)
	, Channels(conf, UL_("Export"), encoderName + UL_("_") + UL_("Channels"), channels)
	, Mode(conf, UL_("Export"), encoderName + UL_("_") + UL_("Mode"), mode)
	, Bitrate(conf, UL_("Export"), encoderName + UL_("_") + UL_("Bitrate"), bitrate)
	, Quality(conf, UL_("Export"), encoderName + UL_("_") + UL_("Quality"), quality)
	, Format2(conf, UL_("Export"), encoderName + UL_("_") + UL_("Format2"), format)
	, Dither(conf, UL_("Export"), encoderName + UL_("_") + UL_("Dither"), dither)
{
	return;
}


EncoderSettingsConf::operator Encoder::Settings() const
{
	Encoder::Settings result;
	result.Cues = Cues;
	result.Tags = Tags;
	result.Samplerate = Samplerate;
	result.Channels = Channels;
	result.Mode = Mode;
	result.Bitrate = Bitrate;
	result.Quality = Quality;
	result.Format = Format2;
	result.Dither = Dither;
	return result;
}


StreamEncoderSettingsConf::StreamEncoderSettingsConf(SettingsContainer &conf, const mpt::ustring &section)
	: FLACCompressionLevel(conf, section, UL_("FLACCompressionLevel"), Encoder::StreamSettings().FLACCompressionLevel)
	, FLACMultithreading(conf, section, UL_("FLACMultithreading"), Encoder::StreamSettings().FLACMultithreading)
	, AUPaddingAlignHint(conf, section, UL_("AUPaddingAlignHint"), Encoder::StreamSettings().AUPaddingAlignHint)
	, MP3ID3v2MinPadding(conf, section, UL_("MP3ID3v2MinPadding"), Encoder::StreamSettings().MP3ID3v2MinPadding)
	, MP3ID3v2PaddingAlignHint(conf, section, UL_("MP3ID3v2PaddingAlignHint"), Encoder::StreamSettings().MP3ID3v2PaddingAlignHint)
	, MP3ID3v2WriteReplayGainTXXX(conf, section, UL_("MP3ID3v2WriteReplayGainTXXX"), Encoder::StreamSettings().MP3ID3v2WriteReplayGainTXXX)
	, MP3LameQuality(conf, section, UL_("MP3LameQuality"), Encoder::StreamSettings().MP3LameQuality)
	, MP3LameID3v2UseLame(conf, section, UL_("MP3LameID3v2UseLame"), Encoder::StreamSettings().MP3LameID3v2UseLame)
	, MP3LameCalculateReplayGain(conf, section, UL_("MP3LameCalculateReplayGain"), Encoder::StreamSettings().MP3LameCalculateReplayGain)
	, MP3LameCalculatePeakSample(conf, section, UL_("MP3LameCalculatePeakSample"), Encoder::StreamSettings().MP3LameCalculatePeakSample)
	, OpusComplexity(conf, section, UL_("OpusComplexity"), Encoder::StreamSettings().OpusComplexity)
{
	return;
}


StreamEncoderSettingsConf::operator Encoder::StreamSettings() const
{
	Encoder::StreamSettings result;
	result.FLACCompressionLevel = FLACCompressionLevel;
	result.FLACMultithreading = FLACMultithreading;
	result.AUPaddingAlignHint = AUPaddingAlignHint;
	result.MP3ID3v2MinPadding = MP3ID3v2MinPadding;
	result.MP3ID3v2PaddingAlignHint = MP3ID3v2PaddingAlignHint;
	result.MP3ID3v2WriteReplayGainTXXX = MP3ID3v2WriteReplayGainTXXX;
	result.MP3LameQuality = MP3LameQuality;
	result.MP3LameID3v2UseLame = MP3LameID3v2UseLame;
	result.MP3LameCalculateReplayGain = MP3LameCalculateReplayGain;
	result.MP3LameCalculatePeakSample = MP3LameCalculatePeakSample;
	result.OpusComplexity = OpusComplexity;
	return result;
}


OPENMPT_NAMESPACE_END
