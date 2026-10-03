/*
 * SampleFormatSFZExt.cpp
 * ----------------------
 * Purpose: SFZ instrument export.
 * Notes  : Copied from upstream's SampleFormatSFZ.cpp. libopenmpt declares CSoundFile::SaveSFZInstrument
 *          but only defines it when external samples are enabled, which the library build never does.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "../soundlib/Sndfile.h"
#include "MPTrackUtil.h"
#include "TrackerSettings.h"
#include "../common/mptFileIO.h"
#include "../soundlib/modsmp_ctrl.h"
#include "mpt/base/numbers.hpp"
#include "mpt/io_file/outputfile.hpp"
#include "mpt/parse/parse.hpp"

#include <functional>

OPENMPT_NAMESPACE_BEGIN

static double SFZLinear2dB(double volume)
{
	return (volume > 0.0 ? 20.0 * std::log10(volume) : -144.0);
}

static void WriteSFZEnvelope(std::ostream &f, double tickDuration, int index, const InstrumentEnvelope &env, const char *type, double scale, std::function<double(int32)> convFunc)
{
	if(!env.dwFlags[ENV_ENABLED] || env.empty())
		return;

	const bool sustainAtEnd = (!env.dwFlags[ENV_SUSTAIN] || env.nSustainStart == (env.size() - 1)) && convFunc(env.back().value) != 0.0;

	const auto prefix = MPT_AFORMAT("\neg{}_")(mpt::afmt::dec0<2>(index));
	f << "\n" << prefix << type << "=" << scale;
	f << prefix << "points=" << (env.size() + (sustainAtEnd ? 1 : 0));
	EnvelopeNode::tick_t lastTick = 0;
	int nodeIndex = 0;
	for(const auto &node : env)
	{
		const double time = (node.tick - lastTick) * tickDuration;
		lastTick = node.tick;
		f << prefix << "time" << nodeIndex << "=" << time;
		f << prefix << "level" << nodeIndex << "=" << convFunc(node.value);
		nodeIndex++;
	}
	if(sustainAtEnd)
	{
		// Prevent envelope from going back to neutral
		f << prefix << "time" << nodeIndex << "=0";
		f << prefix << "level" << nodeIndex << "=" << convFunc(env.back().value);
	}
	// We always must write a sustain point, or the envelope will be sustained on the first point of the envelope
	f << prefix << "sustain=" << (env.dwFlags[ENV_SUSTAIN] ? env.nSustainStart : (env.size() - 1));

	if(env.dwFlags[ENV_LOOP])
		f << "\n// Loop: " << static_cast<uint32>(env.nLoopStart) << "-" << static_cast<uint32>(env.nLoopEnd);
	if(env.dwFlags[ENV_SUSTAIN] && env.nSustainEnd > env.nSustainStart)
		f << "\n// Sustain Loop: " << static_cast<uint32>(env.nSustainStart) << "-" << static_cast<uint32>(env.nSustainEnd);
	if(env.nReleaseNode != ENV_RELEASE_NODE_UNSET)
		f << "\n// Release Node: " << static_cast<uint32>(env.nReleaseNode);
}

static std::string SanitizeSFZString(std::string s, mpt::Charset sourceCharset)
{
	using namespace std::literals;
	// Remove characters could trip up the parser
	std::string::size_type pos = 0;
	while((pos = s.find_first_of("<=\r\n\t\0"sv, pos)) != std::string::npos)
	{
		s[pos++] = ' ';
	}
	return mpt::ToCharset(mpt::Charset::UTF8, sourceCharset, s);
}


bool CSoundFile::SaveSFZInstrument(INSTRUMENTINDEX nInstr, std::ostream &f, const mpt::PathString &filename, bool useFLACsamples) const
{
	const mpt::IO::FlushMode flushMode = mpt::IO::FlushModeFromBool(TrackerSettings::Instance().MiscFlushFileBuffersOnSave);
	const ModInstrument *ins = Instruments[nInstr];
	if(ins == nullptr)
		return false;

	// Creating directory names with trailing spaces or dots is a bad idea, as they are difficult to remove in Windows.
	const mpt::RawPathString whitespaceDirName = PL_(" \n\r\t.");
	const mpt::PathString sampleBaseName = mpt::PathString::FromNative(mpt::trim(filename.GetFilenameBase().AsNative(), whitespaceDirName));
	const mpt::PathString sampleDirName = (sampleBaseName.empty() ? P_("Samples") : sampleBaseName)  + P_("/");
	const mpt::PathString sampleBasePath = filename.GetDirectoryWithDrive() + sampleDirName;
	if(!FileSystem::IsDirectory(sampleBasePath) && !Util::CreateDirectory(sampleBasePath))
		return false;

	const double tickDuration = m_PlayState.m_nSamplesPerTick / static_cast<double>(m_MixerSettings.gdwMixingFreq);

	f << std::setprecision(10);
	f << "// Created with " << mpt::ToCharset(mpt::Charset::UTF8, Version::Current().GetOpenMPTVersionString()) << "\n";
	f << "// Envelope tempo base: tempo " << m_PlayState.m_nMusicTempo.ToDouble();
	switch(m_nTempoMode)
	{
	case TempoMode::Classic:
		f << " (classic tempo mode)";
		break;
	case TempoMode::Alternative:
		f << " (alternative tempo mode)";
		break;
	case TempoMode::Modern:
		f << ", " << m_PlayState.m_nMusicSpeed << " ticks per row, " << m_PlayState.m_nCurrentRowsPerBeat << " rows per beat (modern tempo mode)";
		break;
	case TempoMode::NumModes:
		MPT_ASSERT_NOTREACHED();
		break;
	}

	f << "\n\n<control>\ndefault_path=" << sampleDirName.ToUTF8();
	if(const auto globalName = SanitizeSFZString(ins->name, GetCharsetInternal()); !globalName.empty())
	{
		f << "\n\n<global>\nglobal_label=" << globalName;
	}
	f << "\n\n<group>";
	f << "\nbend_up=" << ins->midiPWD * 100;
	f << "\nbend_down=" << -ins->midiPWD * 100;
	const uint32 cutoff = ins->IsCutoffEnabled() ? ins->GetCutoff() : 127;
	// If filter envelope is active but cutoff is not set, we still need to set the base cutoff frequency to be modulated by the envelope.
	if(ins->IsCutoffEnabled() || ins->PitchEnv.dwFlags[ENV_FILTER])
		f << "\ncutoff=" << CSoundFile::CutOffToFrequency(cutoff) << " // " << cutoff;
	if(ins->IsResonanceEnabled())
		f << "\nresonance=" << Util::muldivr_unsigned(ins->GetResonance(), 24, 128) << " // " << static_cast<int>(ins->GetResonance());
	if(ins->IsCutoffEnabled() || ins->IsResonanceEnabled())
		f << "\nfil_type=" << (ins->filterMode == FilterMode::HighPass ? "hpf_2p" : "lpf_2p");
	if(ins->dwFlags[INS_SETPANNING])
		f << "\npan=" << (Util::muldivr_unsigned(ins->nPan, 200, 256) - 100) << " // " << ins->nPan;
	if(ins->nGlobalVol != 64)
		f << "\nvolume=" << SFZLinear2dB(ins->nGlobalVol / 64.0) << " // " << ins->nGlobalVol;
	if(ins->nFadeOut)
	{
		f << "\nampeg_release=" << (32768.0 * tickDuration / ins->nFadeOut) << " // " << ins->nFadeOut;
		f << "\nampeg_release_shape=0";
	}

	if(ins->nDNA == DuplicateNoteAction::NoteCut && ins->nDCT != DuplicateCheckType::None)
		f << "\npolyphony=1";

	WriteSFZEnvelope(f, tickDuration, 1, ins->VolEnv, "amplitude", 100.0, [](int32 val) { return val / static_cast<double>(ENVELOPE_MAX); });
	WriteSFZEnvelope(f, tickDuration, 2, ins->PanEnv, "pan", 100.0, [](int32 val) { return 2.0 * (val - ENVELOPE_MID) / (ENVELOPE_MAX - ENVELOPE_MIN); });
	if(ins->PitchEnv.dwFlags[ENV_FILTER])
	{
		const auto envScale = 1200.0 * std::log(CutOffToFrequency(127, 256) / static_cast<double>(CutOffToFrequency(0, -256))) / mpt::numbers::ln2;
		const auto cutoffNormal = CutOffToFrequency(cutoff);
		WriteSFZEnvelope(f, tickDuration, 3, ins->PitchEnv, "cutoff", envScale, [this, cutoff, cutoffNormal, envScale](int32 val) {
			// Convert interval between center frequency and envelope into cents
			const auto freq = CutOffToFrequency(cutoff, (val - ENVELOPE_MID) * 256 / (ENVELOPE_MAX - ENVELOPE_MID));
			return 1200.0 * std::log(freq / static_cast<double>(cutoffNormal)) / mpt::numbers::ln2 / envScale;
		});
	} else
	{
		WriteSFZEnvelope(f, tickDuration, 3, ins->PitchEnv, "pitch", 1600.0, [](int32 val) { return 2.0 * (val - ENVELOPE_MID) / (ENVELOPE_MAX - ENVELOPE_MIN); });
	}

	size_t numSamples = 0;
	for(size_t i = 0; i < std::size(ins->Keyboard); i++)
	{
		if(ins->Keyboard[i] < 1 || ins->Keyboard[i] > GetNumSamples())
			continue;

		size_t endOfRegion = i + 1;
		while(endOfRegion < std::size(ins->Keyboard))
		{
			if(ins->Keyboard[endOfRegion] != ins->Keyboard[i] || ins->NoteMap[endOfRegion] != (ins->NoteMap[i] + endOfRegion - i))
				break;
			endOfRegion++;
		}
		endOfRegion--;

		const ModSample &sample = Samples[ins->Keyboard[i]];
		const bool isAdlib = sample.uFlags[CHN_ADLIB];

		if(!sample.HasSampleData())
		{
			i = endOfRegion;
			continue;
		}

		numSamples++;
		mpt::PathString sampleName = sampleBasePath + (sampleBaseName.empty() ? P_("Sample") : sampleBaseName) + P_(" ") + mpt::PathString::FromUnicode(mpt::ufmt::val(numSamples));
		if(isAdlib)
			sampleName += P_(".s3i");
		else if(useFLACsamples)
			sampleName += P_(".flac");
		else
			sampleName += P_(".wav");

		bool success = false;
		try
		{
			mpt::IO::SafeOutputFile sfSmp(sampleName, std::ios::binary, flushMode);
			if(sfSmp)
			{
				mpt::IO::ofstream &fSmp = sfSmp;
				fSmp.exceptions(fSmp.exceptions() | std::ios::badbit | std::ios::failbit);

				if(isAdlib)
					success = SaveS3ISample(ins->Keyboard[i], fSmp);
				else if(useFLACsamples)
					success = SaveFLACSample(ins->Keyboard[i], fSmp);
				else
					success = SaveWAVSample(ins->Keyboard[i], fSmp);
			}
		} catch(const std::exception &)
		{
			success = false;
		}
		if(!success)
		{
			AddToLog(LogError, MPT_USTRING("Unable to save sample: ") + sampleName.ToUnicode());
		}


		f << "\n\n<region>";
		if(const auto regionName = SanitizeSFZString(m_szNames[ins->Keyboard[i]], GetCharsetInternal()); !regionName.empty())
		{
			f << "\nregion_label=" << regionName;
		}
		f << "\nsample=" << sampleName.GetFilename().ToUTF8();
		f << "\nlokey=" << i;
		f << "\nhikey=" << endOfRegion;
		if(sample.rootNote != NOTE_NONE)
			f << "\npitch_keycenter=" << sample.rootNote - NOTE_MIN;
		else
			f << "\npitch_keycenter=" << NOTE_MIDDLEC + i - ins->NoteMap[i];
		if(sample.uFlags[CHN_PANNING])
			f << "\npan=" << (Util::muldivr_unsigned(sample.nPan, 200, 256) - 100) << " // " << sample.nPan;
		if(sample.nGlobalVol != 64)
			f << "\nvolume=" << SFZLinear2dB((ins->nGlobalVol * sample.nGlobalVol) / 4096.0) << " // " << sample.nGlobalVol;
		const char *loopMode = "no_loop", *loopType = "forward";
		SmpLength loopStart = 0, loopEnd = 0;
		if(sample.uFlags[CHN_SUSTAINLOOP])
		{
			loopMode = "loop_sustain";
			loopStart = sample.nSustainStart;
			loopEnd = sample.nSustainEnd;
			if(sample.uFlags[CHN_PINGPONGSUSTAIN])
				loopType = "alternate";
		} else if(sample.uFlags[CHN_LOOP])
		{
			loopMode = "loop_continuous";
			loopStart = sample.nLoopStart;
			loopEnd = sample.nLoopEnd;
			if(sample.uFlags[CHN_PINGPONGLOOP])
				loopType = "alternate";
			else if(sample.uFlags[CHN_REVERSE])
				loopType = "backward";
		}
		f << "\nloop_mode=" << loopMode;
		if(loopStart < loopEnd)
		{
			f << "\nloop_start=" << loopStart;
			f << "\nloop_end=" << (loopEnd - 1);
			f << "\nloop_type=" << loopType;
		}
		if(sample.uFlags.test_all(CHN_SUSTAINLOOP | CHN_LOOP))
		{
			f << "\n// Warning: Only sustain loop was exported!";
		}
		i = endOfRegion;
	}

	return true;
}

OPENMPT_NAMESPACE_END
