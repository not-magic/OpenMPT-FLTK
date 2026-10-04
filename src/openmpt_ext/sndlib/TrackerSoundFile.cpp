// Tracker-only parts of openmpt/soundlib/Sndfile.cpp, Snd_fx.cpp and Sndmix.cpp that libopenmpt does not build.

#include "stdafx.h"
#include "TrackerSoundFile.h"

#include "DlsBankExt.h"
#include "ModSequenceExt.h"
#include "TrackerCriticalSection.h"
#include "MIDIMapping.h"
#include "Moddoc.h"
#include "PluginUi.h"
#include "TrackerSettings.h"
#include "../common/mptFileIO.h"
#include "../soundlib/ITTools.h"
#include "../soundlib/Tables.h"
#include "../soundlib/mod_specifications.h"
#include "../soundlib/plugins/PluginManager.h"
#include "mpt/io_file/inputfile.hpp"

#include <algorithm>
#include <array>
#include <mutex>
#include <unordered_set>

OPENMPT_NAMESPACE_BEGIN

namespace
{

// Applies the DSP effects to everything CSoundFile::Read() renders
class DSPAudioTarget final : public IAudioTarget
{
public:
	DSPAudioTarget(CTrackerSoundFile &sndFile, IAudioTarget &target, void (CTrackerSoundFile::*process)(mpt::audio_span_interleaved<MixSampleInt>))
		: m_sndFile(sndFile), m_target(target), m_process(process)
	{
	}

	void Process(mpt::audio_span_interleaved<MixSampleInt> buffer) override
	{
		(m_sndFile.*m_process)(buffer);
		m_target.Process(buffer);
	}

	void Process(mpt::audio_span_interleaved<MixSampleFloat> buffer) override
	{
		m_target.Process(buffer);
	}

private:
	CTrackerSoundFile &m_sndFile;
	IAudioTarget &m_target;
	void (CTrackerSoundFile::*m_process)(mpt::audio_span_interleaved<MixSampleInt>);
};

// PlayState only exposes the samples left in the current tick to CSoundFile
struct PlayStateAccess : PlayState
{
	static samplecount_t PlayState::*BufferCount() { return &PlayStateAccess::m_nBufferCount; }
};

samplecount_t FindTickFramesRemaining(const PlayState &playState)
{
	return playState.*PlayStateAccess::BufferCount();
}

// libopenmpt reads the paths of external MPTM samples but discards them
std::vector<std::pair<SAMPLEINDEX, mpt::PathString>> FindExternalSamplePaths(FileReader file)
{
	std::vector<std::pair<SAMPLEINDEX, mpt::PathString>> paths;
	file.Rewind();
	ITFileHeader fileHeader;
	if(!file.ReadStruct(fileHeader) || memcmp(fileHeader.id, "IMPM", 4))
		return paths;

	if(fileHeader.cwtv > 0x88A && fileHeader.cwtv <= 0x88D)
	{
		// Deprecated order list format of OpenMPT 1.17.02.46 - 1.17.02.48
		file.Skip(2);
		file.Skip(file.ReadUint32LE() * 4u);
	} else
	{
		file.Skip(fileHeader.ordnum);
	}
	std::vector<uint32le> instrumentPositions, samplePositions;
	if(!file.ReadVector(instrumentPositions, fileHeader.insnum) || !file.ReadVector(samplePositions, fileHeader.smpnum))
		return paths;

	const SAMPLEINDEX numSamples = std::min(static_cast<SAMPLEINDEX>(samplePositions.size()), static_cast<SAMPLEINDEX>(MAX_SAMPLES - 1));
	for(SAMPLEINDEX sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex)
	{
		ITSample sampleHeader;
		if(!samplePositions[sampleIndex] || !file.Seek(samplePositions[sampleIndex]) || !file.ReadStruct(sampleHeader))
			continue;
		if(sampleHeader.cvt != ITSample::cvtExternalSample || !file.Seek(sampleHeader.samplepointer))
			continue;
		size_t filenameLength = 0;
		std::string filenameU8;
		if(file.ReadVarInt(filenameLength) && filenameLength && file.ReadString<mpt::String::maybeNullTerminated>(filenameU8, filenameLength))
			paths.emplace_back(static_cast<SAMPLEINDEX>(sampleIndex + 1), mpt::PathString::FromUTF8(filenameU8));
	}
	return paths;
}

#if defined(MPT_BUILD_DEBUG)
std::mutex g_liveSoundFilesMutex;
std::unordered_set<const CSoundFile *> g_liveSoundFiles;

bool IsTrackerSoundFile(const CSoundFile &sndFile)
{
	const std::lock_guard lock{g_liveSoundFilesMutex};
	return g_liveSoundFiles.count(&sndFile) != 0;
}
#endif

// Upstream runs these on the front and rear mix buffers just before Read() passes them on
void ApplyDSPEffects(CTrackerSoundFile &sndFile, mpt::audio_span_interleaved<MixSampleInt> buffer)
{
	const uint32 dspMask = sndFile.m_MixerSettings.DSPMask & (SNDDSP_SURROUND | SNDDSP_MEGABASS | SNDDSP_EQ | SNDDSP_AGC | SNDDSP_BITCRUSH);
	if(!dspMask)
		return;

	const uint32 numChannels = static_cast<uint32>(buffer.size_channels());
	const uint32 numFrames = static_cast<uint32>(buffer.size_frames());
	MixSampleInt *data = buffer.data();
	// Read() never passes on more than MIXBUFFERSIZE frames at once
	MPT_ASSERT(numFrames <= MIXBUFFERSIZE);
	std::array<MixSampleInt, MIXBUFFERSIZE * 2> front;
	std::array<MixSampleInt, MIXBUFFERSIZE * 2> rear;
	MixSampleInt *frontBuffer = data;
	MixSampleInt *rearBuffer = nullptr;
	if(numChannels == 4)
	{
		for(uint32 frameIndex = 0; frameIndex < numFrames; ++frameIndex)
		{
			front[frameIndex * 2 + 0] = data[frameIndex * 4 + 0];
			front[frameIndex * 2 + 1] = data[frameIndex * 4 + 1];
			rear[frameIndex * 2 + 0] = data[frameIndex * 4 + 2];
			rear[frameIndex * 2 + 1] = data[frameIndex * 4 + 3];
		}
		frontBuffer = front.data();
		rearBuffer = rear.data();
	}

	if(dspMask & SNDDSP_SURROUND)
		sndFile.m_Surround.Process(frontBuffer, rearBuffer, numFrames, numChannels);
	if(dspMask & SNDDSP_MEGABASS)
		sndFile.m_MegaBass.Process(frontBuffer, rearBuffer, numFrames, numChannels);
	if(dspMask & SNDDSP_EQ)
		sndFile.m_EQ.Process(frontBuffer, rearBuffer, numFrames, numChannels);
	if(dspMask & SNDDSP_AGC)
		sndFile.m_AGC.Process(frontBuffer, rearBuffer, numFrames, numChannels);
	if(dspMask & SNDDSP_BITCRUSH)
		sndFile.m_BitCrush.Process(frontBuffer, rearBuffer, numFrames, numChannels);

	if(numChannels == 4)
	{
		for(uint32 frameIndex = 0; frameIndex < numFrames; ++frameIndex)
		{
			data[frameIndex * 4 + 0] = front[frameIndex * 2 + 0];
			data[frameIndex * 4 + 1] = front[frameIndex * 2 + 1];
			data[frameIndex * 4 + 2] = rear[frameIndex * 2 + 0];
			data[frameIndex * 4 + 3] = rear[frameIndex * 2 + 1];
		}
	}
}


// Mixes the metronome click after all effects, as upstream does
void MixMetronome(const ModSample &sample, double &position, double increment, mpt::audio_span_interleaved<MixSampleInt> buffer)
{
	const std::size_t numChannels = buffer.size_channels();
	const int32 volume = sample.nVolume * 16;
	const bool isStereo = sample.uFlags[CHN_STEREO];
	const bool is16Bit = sample.uFlags[CHN_16BIT];
	for(std::size_t frameIndex = 0; frameIndex < buffer.size_frames() && position < sample.nLength; ++frameIndex)
	{
		const std::size_t samplePos = static_cast<std::size_t>(position);
		const std::size_t leftIndex = isStereo ? samplePos * 2 : samplePos;
		const std::size_t rightIndex = isStereo ? samplePos * 2 + 1 : samplePos;
		const int32 leftValue = is16Bit ? sample.sample16()[leftIndex] : sample.sample8()[leftIndex] * 256;
		const int32 rightValue = is16Bit ? sample.sample16()[rightIndex] : sample.sample8()[rightIndex] * 256;
		buffer(0, frameIndex) += leftValue * volume;
		if(numChannels > 1)
			buffer(1, frameIndex) += rightValue * volume;
		position += increment;
	}
}

}  // namespace

const NoteName *CTrackerSoundFile::m_NoteNames = NoteNamesFlat;


CTrackerSoundFile::CTrackerSoundFile()
	: m_MIDIMapper{std::make_unique<CMIDIMapper>(*this)}
{
	const TrackerSettings &settings = TrackerSettings::Instance();
	const uint32 rowsPerBeat = settings.m_nRowHighlightBeats ? settings.m_nRowHighlightBeats : DEFAULT_ROWS_PER_BEAT;
	const uint32 rowsPerMeasure = settings.m_nRowHighlightMeasures >= rowsPerBeat ? settings.m_nRowHighlightMeasures : rowsPerBeat * 4;
	m_nDefaultRowsPerBeat = m_PlayState.m_nCurrentRowsPerBeat = rowsPerBeat;
	m_nDefaultRowsPerMeasure = m_PlayState.m_nCurrentRowsPerMeasure = rowsPerMeasure;
	m_PluginManager = std::make_unique<CVstPluginManager>();
	PluginUi::RegisterTrackerPlugins(*m_PluginManager);
#if defined(MPT_BUILD_DEBUG)
	const std::lock_guard lock{g_liveSoundFilesMutex};
	g_liveSoundFiles.insert(this);
#endif
}


CTrackerSoundFile::~CTrackerSoundFile()
{
	PluginUi::CloseAllEditors(*this);
#if defined(MPT_BUILD_DEBUG)
	const std::lock_guard lock{g_liveSoundFilesMutex};
	g_liveSoundFiles.erase(this);
#endif
}


void CTrackerSoundFile::Create(MODTYPE type, CHANNELINDEX numChannels, CModDoc *pModDoc)
{
	m_pModDoc = pModDoc;
	m_samplePaths.clear();
	m_channelColors.clear();
	CSoundFile::Create(type, numChannels, pModDoc);
}


bool CTrackerSoundFile::Create(FileReader file, ModLoadingFlags loadFlags, CModDoc *pModDoc)
{
	m_pModDoc = pModDoc;
	m_samplePaths.clear();
	m_channelColors.clear();
	if(!CSoundFile::Create(file, loadFlags, pModDoc))
		return false;
	if(GetType() != MOD_TYPE_MPT || !(loadFlags & loadSampleData))
		return true;

	mpt::PathString baseDir;
	if(const auto fileName = file.GetOptionalFileName(); fileName.has_value())
		baseDir = fileName->GetDirectoryWithDrive();
	else if(pModDoc != nullptr)
		baseDir = pModDoc->GetPathNameMpt().GetDirectoryWithDrive();
	for(const auto &[sampleIndex, path] : FindExternalSamplePaths(file))
	{
		if(sampleIndex > GetNumSamples() || !Samples[sampleIndex].uFlags[SMP_KEEPONDISK])
			continue;
		// Failures are reported by CModDoc, as in the tracker build
		LoadExternalSample(sampleIndex, mpt::RelativePathToAbsolute(path, baseDir).Simplify());
		if(Samples[sampleIndex].HasSampleData())
			Samples[sampleIndex].PrecomputeLoops(*this, false);
	}
	return true;
}


void CTrackerSoundFile::Destroy()
{
	PluginUi::CloseAllEditors(*this);
	m_samplePaths.clear();
	CSoundFile::Destroy();
}


void CTrackerSoundFile::ChangeModTypeTo(const MODTYPE newType, bool isAdjusting)
{
	const MODTYPE oldType = GetType();
	const PlayBehaviourSet oldBehaviour = m_playBehaviour;
	SetType(newType);
	m_playBehaviour = oldBehaviour;

	if(oldType == newType || !isAdjusting)
		return;

	SetupMODPanning();

	const PlayBehaviourSet oldAllowedFlags = GetSupportedPlaybackBehaviour(oldType);
	const PlayBehaviourSet newAllowedFlags = GetSupportedPlaybackBehaviour(newType);
	const PlayBehaviourSet newDefaultFlags = GetDefaultPlaybackBehaviour(newType);
	for(size_t i = 0; i < m_playBehaviour.size(); ++i)
	{
		if(m_playBehaviour[i])
			m_playBehaviour.set(i, newAllowedFlags[i]);
		if(!oldAllowedFlags[i])
			m_playBehaviour.set(i, newDefaultFlags[i]);
	}
	// Retain S3M-like note-off behaviour when converting from S3M to MPTM
	if(oldType == MOD_TYPE_S3M && newType == MOD_TYPE_MPT && m_opl)
		m_playBehaviour.reset(kOPLFlexibleNoteOff);

	OnSequencesModTypeChanged(*this, oldType);
	Patterns.OnModTypeChanged(oldType);

	m_modFormat.type = GetModSpecifications().GetFileExtension();
}


void CTrackerSoundFile::SetDefaultNoteNames()
{
	m_NoteNames = TrackerSettings::Instance().accidentalFlats ? NoteNamesFlat : NoteNamesSharp;
}


const NoteName *CTrackerSoundFile::GetDefaultNoteNames()
{
	return m_NoteNames;
}


mpt::ustring CTrackerSoundFile::GetDefaultNoteName(int noteIndex)
{
	return m_NoteNames[noteIndex];
}


mpt::ustring CTrackerSoundFile::GetNoteName(const ModCommand::NOTE note, const INSTRUMENTINDEX instrumentIndex, const NoteName *noteNames) const
{
	return CSoundFile::GetNoteName(note, instrumentIndex, noteNames ? noteNames : m_NoteNames);
}


mpt::ustring CTrackerSoundFile::GetNoteName(const ModCommand::NOTE note) const
{
	return CSoundFile::GetNoteName(note, m_NoteNames);
}


mpt::ustring CTrackerSoundFile::GetNoteName(const ModCommand::NOTE note, const NoteName *noteNames)
{
	return CSoundFile::GetNoteName(note, noteNames);
}


ChannelFlags CTrackerSoundFile::GetChannelMuteFlag()
{
	return (TrackerSettings::Instance().patternSetup & PatternSetup::SyncMute) ? CHN_SYNCMUTE : CHN_MUTE;
}


uint32 CTrackerSoundFile::GetChannelColor(CHANNELINDEX channelIndex) const noexcept
{
	return channelIndex < m_channelColors.size() ? m_channelColors[channelIndex] : INVALID_CHANNEL_COLOR;
}


void CTrackerSoundFile::SetChannelColor(CHANNELINDEX channelIndex, uint32 colorValue)
{
	if(channelIndex >= m_channelColors.size())
	{
		if(colorValue == INVALID_CHANNEL_COLOR)
			return;
		m_channelColors.resize(channelIndex + 1, INVALID_CHANNEL_COLOR);
	}
	m_channelColors[channelIndex] = colorValue;
}


void CTrackerSoundFile::RearrangeChannelColors(const std::vector<CHANNELINDEX> &newOrder)
{
	std::vector<uint32> newColors(newOrder.size(), INVALID_CHANNEL_COLOR);
	for(size_t channelIndex = 0; channelIndex < newOrder.size(); ++channelIndex)
	{
		newColors[channelIndex] = GetChannelColor(newOrder[channelIndex]);
	}
	m_channelColors = std::move(newColors);
}


void CTrackerSoundFile::SetSamplePath(SAMPLEINDEX sampleIndex, mpt::PathString filename)
{
	if(m_samplePaths.size() < sampleIndex)
		m_samplePaths.resize(sampleIndex);
	m_samplePaths[sampleIndex - 1] = std::move(filename);
}


void CTrackerSoundFile::ResetSamplePath(SAMPLEINDEX sampleIndex)
{
	if(m_samplePaths.size() >= sampleIndex)
		m_samplePaths[sampleIndex - 1] = mpt::PathString();
	Samples[sampleIndex].uFlags.reset(SMP_KEEPONDISK | SMP_MODIFIED);
}


mpt::PathString CTrackerSoundFile::GetSamplePath(SAMPLEINDEX sampleIndex) const
{
	if(m_samplePaths.size() >= sampleIndex)
		return m_samplePaths[sampleIndex - 1];
	return mpt::PathString();
}


bool CTrackerSoundFile::SampleHasPath(SAMPLEINDEX sampleIndex) const
{
	return m_samplePaths.size() >= sampleIndex && !m_samplePaths[sampleIndex - 1].empty();
}


bool CTrackerSoundFile::IsExternalSampleMissing(SAMPLEINDEX sampleIndex) const
{
	return Samples[sampleIndex].uFlags[SMP_KEEPONDISK] && !Samples[sampleIndex].HasSampleData();
}


bool CTrackerSoundFile::LoadExternalSample(SAMPLEINDEX sampleIndex, const mpt::PathString &filename)
{
	bool isLoaded = false;
	mpt::IO::InputFile inputFile(filename, TrackerSettings::Instance().MiscCacheCompleteFileBeforeLoading);
	if(inputFile.IsValid())
	{
		const ModSample origSample = Samples[sampleIndex];
		mpt::charbuf<MAX_SAMPLENAME> origName;
		origName = m_szNames[sampleIndex];

		FileReader file = GetFileReader(inputFile);
		isLoaded = ReadSampleFromFile(sampleIndex, file, false);
		if(isLoaded)
		{
			// Keep the new sample data but the old sample properties
			ModSample &sample = GetSample(sampleIndex);
			const SmpLength newLength = sample.nLength;
			void *newData = sample.samplev();
			const SampleFlags newFlags = sample.uFlags;

			sample = origSample;
			sample.nLength = newLength;
			sample.pData.pSample = newData;
			sample.uFlags.set(CHN_16BIT, newFlags[CHN_16BIT]);
			sample.uFlags.set(CHN_STEREO, newFlags[CHN_STEREO]);
			sample.uFlags.reset(SMP_MODIFIED);
			sample.SanitizeLoops();
		}
		m_szNames[sampleIndex] = origName;
	}
	SetSamplePath(sampleIndex, filename);
	return isLoaded;
}


bool CTrackerSoundFile::DestroyInstrument(INSTRUMENTINDEX instrumentIndex, deleteInstrumentSamples removeSamples)
{
	TrackerCriticalSection cs;
	return CSoundFile::DestroyInstrument(instrumentIndex, removeSamples);
}


SAMPLEINDEX CTrackerSoundFile::RemoveSelectedSamples(const std::vector<bool> &keepSamples)
{
	TrackerCriticalSection cs;
	return CSoundFile::RemoveSelectedSamples(keepSamples);
}


bool CTrackerSoundFile::DestroySampleThreadsafe(SAMPLEINDEX sampleIndex)
{
	TrackerCriticalSection cs;
	return CSoundFile::DestroySampleThreadsafe(sampleIndex);
}


bool CTrackerSoundFile::ReadSampleFromFile(SAMPLEINDEX sampleIndex, FileReader &file, bool mayNormalize, bool isIncludingInstrumentFormats)
{
	if(CSoundFile::ReadSampleFromFile(sampleIndex, file, mayNormalize, isIncludingInstrumentFormats))
		return true;
	if(!sampleIndex || sampleIndex >= MAX_SAMPLES)
		return false;

	file.Rewind();
	if(!ReadFLACSample(sampleIndex, file))
		return false;
	if(sampleIndex > GetNumSamples())
		m_nSamples = sampleIndex;
	return true;
}


bool CTrackerSoundFile::ReadInstrumentFromFile(INSTRUMENTINDEX instrumentIndex, FileReader &file, bool mayNormalize)
{
	if(CSoundFile::ReadInstrumentFromFile(instrumentIndex, file, mayNormalize))
		return true;
	if(!instrumentIndex || instrumentIndex >= MAX_INSTRUMENTS)
		return false;

	file.Rewind();
	CDLSBank bank;
	if(!bank.Open(file) || !bank.ExtractInstrument(*this, instrumentIndex, 0, 0))
		return false;
	if(instrumentIndex > GetNumInstruments())
		m_nInstruments = instrumentIndex;
	return true;
}


bool CTrackerSoundFile::IsOrderPositionLocked(ORDERINDEX orderIndex) const noexcept
{
	return m_lockOrderStart != ORDERINDEX_INVALID && (orderIndex < m_lockOrderStart || orderIndex > m_lockOrderEnd);
}


void CTrackerSoundFile::PatternTranstionChnSolo(const CHANNELINDEX firstChannelIndex, const CHANNELINDEX lastChannelIndex)
{
	if(firstChannelIndex >= GetNumChannels() || lastChannelIndex < firstChannelIndex)
		return;

	for(CHANNELINDEX i = 0; i < GetNumChannels(); ++i)
	{
		const bool isMuted = ChnSettings[i].dwFlags[CHN_MUTE];
		m_bChannelMuteTogglePending[i] = (i >= firstChannelIndex && i <= lastChannelIndex) ? isMuted : !isMuted;
	}
}


void CTrackerSoundFile::PatternTransitionChnUnmuteAll()
{
	for(CHANNELINDEX i = 0; i < GetNumChannels(); ++i)
	{
		m_bChannelMuteTogglePending[i] = ChnSettings[i].dwFlags[CHN_MUTE];
	}
}


void CTrackerSoundFile::InitPlayer(bool isReset)
{
	CSoundFile::InitPlayer(isReset);
	const uint32 mixingFreq = m_MixerSettings.gdwMixingFreq;
	m_Surround.Initialize(isReset, mixingFreq);
	m_MegaBass.Initialize(isReset, mixingFreq);
	m_EQ.Initialize(isReset, mixingFreq);
	m_AGC.Initialize(isReset, mixingFreq);
	m_BitCrush.Initialize(isReset, mixingFreq);
}


void CTrackerSoundFile::SetDspEffects(uint32 dspMask)
{
	m_MixerSettings.DSPMask = dspMask;
	InitPlayer(false);
}


void CTrackerSoundFile::SetPreAmp(uint32 preAmpValue)
{
	const uint32 clampedPreAmp = std::clamp(preAmpValue, uint32(1), uint32(0x200));
	if(clampedPreAmp < m_MixerSettings.m_nPreAmp && (m_MixerSettings.DSPMask & SNDDSP_AGC))
		m_AGC.Adjust(m_MixerSettings.m_nPreAmp, clampedPreAmp);
	CSoundFile::SetPreAmp(preAmpValue);
}


void CTrackerSoundFile::SetEQGains(const uint32 *pGains, const uint32 *pFreqs, bool isReset)
{
	m_EQ.SetEQGains(pGains, pFreqs, isReset, m_MixerSettings.gdwMixingFreq);
}


void CTrackerSoundFile::SetMetronomeSamples(const ModSample *measure, const ModSample *beat)
{
	m_metronomeMeasure = measure;
	m_metronomeBeat = beat;
	m_metronomePlaying = nullptr;
}


samplecount_t CTrackerSoundFile::Render(samplecount_t count, IAudioTarget &target)
{
	AudioSourceNone source;
	return Render(count, target, source);
}


samplecount_t CTrackerSoundFile::Render(samplecount_t count, IAudioTarget &target, IAudioSource &source, std::optional<std::reference_wrapper<IMonitorOutput>> outputMonitor, std::optional<std::reference_wrapper<IMonitorInput>> inputMonitor)
{
	DSPAudioTarget dspTarget{*this, target, &CTrackerSoundFile::ProcessOutput};
	samplecount_t renderedTotal = 0;
	while(renderedTotal < count)
	{
		samplecount_t chunkSize = FindTickFramesRemaining(m_PlayState);
		const bool isTickStart = (chunkSize == 0);
		if(isTickStart)
		{
			// The next Read() starts a new tick, and its length is only known once it has started
			OnTickBoundary();
			EmulatePause();
			chunkSize = 1;
		}
		chunkSize = std::min(chunkSize, count - renderedTotal);
		const samplecount_t rendered = Read(chunkSize, dspTarget, source, outputMonitor, inputMonitor);
		if(isTickStart && rendered > 0 && m_PatternCuePoints != nullptr && (m_PatternCuePoints->empty() || m_PlayState.m_nCurrentOrder != m_PatternCuePoints->back().orderIndex))
		{
			// For WAV export; the offset is relative to this call and completed by the caller
			m_PatternCuePoints->push_back({renderedTotal, m_PlayState.m_nCurrentOrder, false});
		}
		renderedTotal += rendered;
		if(rendered < chunkSize)
			break;
	}
	return renderedTotal;
}


// ReadNote() only skips ProcessRow() while SONG_PAUSED in the tracker build. Here ProcessRow() is kept
// on a mid-row tick without pending effects, so only the preview and sustaining channels are mixed.
void CTrackerSoundFile::EmulatePause()
{
	if(!m_PlayState.m_flags[SONG_PAUSED])
	{
		if(m_savedSpeedValue >= 0)
			m_PlayState.m_nMusicSpeed = static_cast<uint16>(m_savedSpeedValue);
		m_savedSpeedValue = -1;
		m_isPauseEmulated = false;
		return;
	}
	if(!m_isPauseEmulated)
	{
		m_isPauseEmulated = true;
		for(ModChannel &chn : m_PlayState.PatternChannels(*this))
		{
			chn.nCommand = CMD_NONE;
			chn.rowCommand = ModCommand();
		}
	}
	if(m_PlayState.TicksOnRow() < 2)
	{
		if(m_savedSpeedValue < 0)
			m_savedSpeedValue = m_PlayState.m_nMusicSpeed;
		m_PlayState.m_nMusicSpeed = 2;
	}
	m_PlayState.m_nTickCount = 0;
}


// Upstream applies these right after CSoundFile::NextRow(), which only copies m_nNextRow and
// m_nNextOrder into the current position. Adjusting the "next" position before the tick starts
// therefore has the same effect.
void CTrackerSoundFile::OnTickBoundary()
{
	const bool isEndOfRow = m_PlayState.m_nTickCount + 1 >= m_PlayState.TicksOnRow();
	if(!isEndOfRow || m_PlayState.m_flags[SONG_PAUSED])
		return;

	if(m_PlayState.m_flags[SONG_STEP])
	{
		m_PlayState.m_flags.reset(SONG_STEP);
		m_PlayState.m_flags.set(SONG_PAUSED);
		return;
	}

	HandleRowTransitionEvents();

	if(IsRenderingToDisc())
		return;
	if(m_lockRowStart != ROWINDEX_INVALID && (m_PlayState.m_nNextRow < m_lockRowStart || m_PlayState.m_nNextRow > m_lockRowEnd))
		m_PlayState.m_nNextRow = m_lockRowStart;
	if(IsOrderPositionLocked(m_PlayState.m_nNextOrder))
		m_PlayState.m_nNextOrder = m_lockOrderStart;
}


void CTrackerSoundFile::HandleRowTransitionEvents()
{
	const bool isNextPattern = m_PlayState.m_nNextRow == 0 || m_PlayState.m_flags[SONG_BREAKTOROW];
	const ROWINDEX nextRowIndex = m_PlayState.m_nNextRow;
	bool isTransitionDue = isNextPattern;

	if(m_PlayState.m_nSeqOverride != ORDERINDEX_INVALID && m_PlayState.m_nSeqOverride < Order().size())
	{
		switch(m_PlayState.m_seqOverrideMode)
		{
		case OrderTransitionMode::AtPatternEnd:
			isTransitionDue = isNextPattern;
			break;
		case OrderTransitionMode::AtMeasureEnd:
			if(m_PlayState.m_nCurrentRowsPerMeasure > 0)
				isTransitionDue = (nextRowIndex % m_PlayState.m_nCurrentRowsPerMeasure) == 0;
			break;
		case OrderTransitionMode::AtBeatEnd:
			if(m_PlayState.m_nCurrentRowsPerBeat > 0)
				isTransitionDue = (nextRowIndex % m_PlayState.m_nCurrentRowsPerBeat) == 0;
			break;
		case OrderTransitionMode::AtRowEnd:
			isTransitionDue = true;
			break;
		}
		if(isTransitionDue)
		{
			if(m_PlayState.m_flags[SONG_PATTERNLOOP])
				m_PlayState.m_nPattern = Order()[m_PlayState.m_nSeqOverride];
			m_PlayState.m_nNextOrder = m_PlayState.m_nSeqOverride;
			m_PlayState.m_nSeqOverride = ORDERINDEX_INVALID;
		}
	}

	if(isTransitionDue && m_pModDoc)
	{
		for(CHANNELINDEX channelIndex = 0; channelIndex < GetNumChannels(); ++channelIndex)
		{
			if(m_bChannelMuteTogglePending[channelIndex])
			{
				m_pModDoc->MuteChannel(channelIndex, !m_pModDoc->IsChannelMuted(channelIndex));
				m_bChannelMuteTogglePending[channelIndex] = false;
			}
		}
	}

	if(!IsMetronomeEnabled() || IsRenderingToDisc() || m_PlayState.m_flags[SONG_PAUSED | SONG_STEP])
	{
		m_metronomePlaying = nullptr;
		return;
	}
	const ROWINDEX rowsPerMeasure = m_PlayState.m_nCurrentRowsPerMeasure ? m_PlayState.m_nCurrentRowsPerMeasure : DEFAULT_ROWS_PER_MEASURE;
	const ROWINDEX rowsPerBeat = m_PlayState.m_nCurrentRowsPerBeat ? m_PlayState.m_nCurrentRowsPerBeat : DEFAULT_ROWS_PER_BEAT;
	const ModSample *sample = nullptr;
	if(!m_PlayState.m_lTotalSampleCount || !(nextRowIndex % rowsPerMeasure))
		sample = m_metronomeMeasure;
	else if(!(nextRowIndex % rowsPerMeasure % rowsPerBeat))
		sample = m_metronomeBeat;
	if(sample && sample->HasSampleData())
	{
		m_metronomePlaying = sample;
		m_metronomePosition = 0.0;
	}
}


void CTrackerSoundFile::ProcessOutput(mpt::audio_span_interleaved<MixSampleInt> buffer)
{
	ApplyDSPEffects(*this, buffer);
	if(m_metronomePlaying)
	{
		const double increment = static_cast<double>(m_metronomePlaying->nC5Speed) / m_MixerSettings.gdwMixingFreq;
		MixMetronome(*m_metronomePlaying, m_metronomePosition, increment, buffer);
		if(m_metronomePosition >= m_metronomePlaying->nLength)
			m_metronomePlaying = nullptr;
	}
}


CTrackerSoundFile &TrackerSoundFile(CSoundFile &sndFile)
{
#if defined(MPT_BUILD_DEBUG)
	MPT_ASSERT_ALWAYS(IsTrackerSoundFile(sndFile));
#endif
	return static_cast<CTrackerSoundFile &>(sndFile);
}


const CTrackerSoundFile &TrackerSoundFile(const CSoundFile &sndFile)
{
#if defined(MPT_BUILD_DEBUG)
	MPT_ASSERT_ALWAYS(IsTrackerSoundFile(sndFile));
#endif
	return static_cast<const CTrackerSoundFile &>(sndFile);
}

OPENMPT_NAMESPACE_END
