// Tracker-only parts of openmpt/soundlib/ModSequence.h and pattern.h as free functions.

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../soundlib/Snd_defs.h"

#include <vector>

OPENMPT_NAMESPACE_BEGIN

class CPattern;
class CSoundFile;
class ModSequence;

// Check if this sequence has subsongs separated by invalid ("---" or non-existing) patterns
bool HasSubsongs(const ModSequence &order, const CSoundFile &sndFile) noexcept;

// newOrder lists which existing sequences to keep or duplicate, SEQUENCEINDEX_INVALID inserts an empty one
bool RearrangeSequences(CSoundFile &sndFile, const std::vector<SEQUENCEINDEX> &newOrder);
void OnSequencesModTypeChanged(CSoundFile &sndFile, MODTYPE oldType);
bool CanSplitSubsongs(const CSoundFile &sndFile) noexcept;
bool SplitSubsongsToMultipleSequences(CSoundFile &sndFile);
// Convert the sequence's restart position and tempo information to pattern commands
bool WriteGlobalsToPattern(CSoundFile &sndFile, SEQUENCEINDEX sequenceIndex, bool isWritingRestartPos, bool isWritingTempo);
// Merges all sequences into the first one
bool MergeSequences(CSoundFile &sndFile);

// Double / halve the number of rows
bool ExpandPattern(CPattern &pattern);
bool ShrinkPattern(CPattern &pattern);

OPENMPT_NAMESPACE_END
