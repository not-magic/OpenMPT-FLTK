// FLTK port of openmpt/mptrack/ModConvert.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

OPENMPT_NAMESPACE_BEGIN

// Warning types
enum ConversionWarning
{
	wInstrumentsToSamples = 0,
	wResizedPatterns,
	wSampleBidiLoops,
	wSampleSustainLoops,
	wSampleAutoVibrato,
	wMODSampleFrequency,
	wBrokenNoteMap,
	wInstrumentSustainLoops,
	wInstrumentTuning,
	wMODGlobalVars,
	wMOD31Samples,
	wAdlibInstruments,
	wRestartPos,
	wChannelVolSurround,
	wChannelPanning,
	wPatternSignatures,
	wLinearSlides,
	wTrimmedEnvelopes,
	wReleaseNode,
	wEditHistory,
	wMixmode,
	wVolRamp,
	wPitchToTempoLock,
	wGlobalVolumeNotSupported,
	wFilterVariation,
	wResamplingMode,
	wFractionalTempo,
	wNumWarnings
};

OPENMPT_NAMESPACE_END
