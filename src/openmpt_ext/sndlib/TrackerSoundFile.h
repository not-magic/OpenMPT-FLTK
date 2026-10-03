/*
 * TrackerSoundFile.h
 * ------------------
 * Purpose: CSoundFile with the editor state that upstream only compiles into the tracker build.
 * Notes  : libopenmpt is built without MODPLUG_TRACKER, so everything here lives outside the engine.
 *          Every CSoundFile the GUI creates must be a CTrackerSoundFile.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../soundlib/Sndfile.h"
#include "openmpt_ext/sounddsp/SoundDspExt.h"

#include <bitset>
#include <memory>
#include <optional>
#include <vector>

OPENMPT_NAMESPACE_BEGIN

class CMIDIMapper;
class CModDoc;

// For WAV export (writing pattern positions to file)
struct PatternCuePoint
{
	uint64 offset;     // offset in the file (in samples)
	ORDERINDEX orderIndex;  // which order is this?
	bool isProcessed;    // has this point been processed by the main WAV render function yet?
};

class CTrackerSoundFile : public CSoundFile
{
public:
	static constexpr uint32 INVALID_CHANNEL_COLOR = 0xFFFFFFFF;

	CTrackerSoundFile();
	~CTrackerSoundFile();

	CTrackerSoundFile(const CTrackerSoundFile &) = delete;
	CTrackerSoundFile &operator=(const CTrackerSoundFile &) = delete;

	void Create(MODTYPE type, CHANNELINDEX numChannels, CModDoc *pModDoc = nullptr);
	bool Create(FileReader file, ModLoadingFlags loadFlags = loadCompleteModule, CModDoc *pModDoc = nullptr);
	void Destroy();

	CModDoc *GetpModDoc() const noexcept { return m_pModDoc; }

	void ChangeModTypeTo(const MODTYPE newType, bool isAdjusting = true);

	static void SetDefaultNoteNames();
	static const NoteName *GetDefaultNoteNames();
	static mpt::ustring GetDefaultNoteName(int noteIndex);  // note = [0..11]
	mpt::ustring GetNoteName(const ModCommand::NOTE note, const INSTRUMENTINDEX instrumentIndex, const NoteName *noteNames = nullptr) const;
	mpt::ustring GetNoteName(const ModCommand::NOTE note) const;
	static mpt::ustring GetNoteName(const ModCommand::NOTE note, const NoteName *noteNames);

	static ChannelFlags GetChannelMuteFlag();

	uint32 GetChannelColor(CHANNELINDEX channelIndex) const noexcept;
	void SetChannelColor(CHANNELINDEX channelIndex, uint32 colorValue);
	bool HasChannelColor(CHANNELINDEX channelIndex) const noexcept { return GetChannelColor(channelIndex) != INVALID_CHANNEL_COLOR; }
	// newOrder[i] is the old index of channel i, or CHANNELINDEX_INVALID for a new channel
	void RearrangeChannelColors(const std::vector<CHANNELINDEX> &newOrder);

	CMIDIMapper &GetMIDIMapper() { return *m_MIDIMapper; }
	const CMIDIMapper &GetMIDIMapper() const { return *m_MIDIMapper; }

	void SetSamplePath(SAMPLEINDEX sampleIndex, mpt::PathString filename);
	void ResetSamplePath(SAMPLEINDEX sampleIndex);
	mpt::PathString GetSamplePath(SAMPLEINDEX sampleIndex) const;
	bool SampleHasPath(SAMPLEINDEX sampleIndex) const;
	bool IsExternalSampleMissing(SAMPLEINDEX sampleIndex) const;
	bool LoadExternalSample(SAMPLEINDEX sampleIndex, const mpt::PathString &filename);

	// These lock the audio thread out, as upstream's tracker build does internally
	bool DestroyInstrument(INSTRUMENTINDEX instrumentIndex, deleteInstrumentSamples removeSamples);
	SAMPLEINDEX RemoveSelectedSamples(const std::vector<bool> &keepSamples);
	bool DestroySampleThreadsafe(SAMPLEINDEX sampleIndex);

	// Also accepts FLAC, which libopenmpt is never built with
	bool ReadSampleFromFile(SAMPLEINDEX sampleIndex, FileReader &file, bool mayNormalize = false, bool isIncludingInstrumentFormats = true);
	bool ReadFLACSample(SAMPLEINDEX sampleIndex, FileReader &file);
	bool SaveFLACSample(SAMPLEINDEX sampleIndex, std::ostream &f) const;

	// Also accepts DLS / SF2 banks, which libopenmpt only loads in the tracker build
	bool ReadInstrumentFromFile(INSTRUMENTINDEX instrumentIndex, FileReader &file, bool mayNormalize);

	bool IsOrderPositionLocked(ORDERINDEX orderIndex) const noexcept;
	void PatternTranstionChnSolo(const CHANNELINDEX firstChannelIndex, const CHANNELINDEX lastChannelIndex);
	void PatternTransitionChnUnmuteAll();

	// The DSP effects are applied by Render(), as libopenmpt is built without them
	void InitPlayer(bool isReset = false);
	void SetDspEffects(uint32 dspMask);
	void SetPreAmp(uint32 preAmpValue);
	void SetEQGains(const uint32 *pGains, const uint32 *pFreqs, bool isReset = false);

	void SetMetronomeSamples(const ModSample *measure, const ModSample *beat);
	bool IsMetronomeEnabled() const noexcept { return m_metronomeMeasure || m_metronomeBeat; }

	// Read() that stops at every tick boundary to apply the tracker's row transition rules
	samplecount_t Render(samplecount_t count, IAudioTarget &target, IAudioSource &source, std::optional<std::reference_wrapper<IMonitorOutput>> outputMonitor = std::nullopt, std::optional<std::reference_wrapper<IMonitorInput>> inputMonitor = std::nullopt);
	samplecount_t Render(samplecount_t count, IAudioTarget &target);

public:
	ROWINDEX m_lockRowStart = ROWINDEX_INVALID, m_lockRowEnd = ROWINDEX_INVALID;
	ORDERINDEX m_lockOrderStart = ORDERINDEX_INVALID, m_lockOrderEnd = ORDERINDEX_INVALID;

	std::bitset<MAX_BASECHANNELS> m_bChannelMuteTogglePending;
	std::bitset<MAX_MIXPLUGINS> m_pluginDryWetRatioChanged;

	CSurround m_Surround;
	CMegaBass m_MegaBass;
	CEQ m_EQ;
	CAGC m_AGC;
	BitCrush m_BitCrush;

	std::vector<PatternCuePoint> *m_PatternCuePoints = nullptr;
	std::vector<SmpLength> *m_SamplePlayLengths = nullptr;

private:
	void OnTickBoundary();
	void ProcessOutput(mpt::audio_span_interleaved<MixSampleInt> buffer);
	void HandleRowTransitionEvents();

	static const NoteName *m_NoteNames;

	CModDoc *m_pModDoc = nullptr;
	std::unique_ptr<CMIDIMapper> m_MIDIMapper;
	std::vector<mpt::PathString> m_samplePaths;
	std::vector<uint32> m_channelColors;
	const ModSample *m_metronomeMeasure = nullptr;
	const ModSample *m_metronomeBeat = nullptr;
	const ModSample *m_metronomePlaying = nullptr;
	double m_metronomePosition = 0.0;
};

CTrackerSoundFile &TrackerSoundFile(CSoundFile &sndFile);
const CTrackerSoundFile &TrackerSoundFile(const CSoundFile &sndFile);

OPENMPT_NAMESPACE_END
