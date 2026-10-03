/*
 * ModSequenceExt.cpp
 * ------------------
 * Purpose: Tracker-only order list and pattern editing operations.
 * Notes  : Ported from upstream's ModSequence.cpp and pattern.cpp, using only the public API.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ModSequenceExt.h"

#include "../soundlib/Sndfile.h"
#include "../soundlib/mod_specifications.h"

#include <algorithm>

OPENMPT_NAMESPACE_BEGIN

namespace
{

void RemoveSequencesAfterFirst(ModSequenceSet &sequences)
{
	sequences.SetSequence(0);
	while(sequences.GetNumSequences() > 1)
	{
		sequences.RemoveSequence(sequences.GetNumSequences() - 1);
	}
}

}  // namespace


bool HasSubsongs(const ModSequence &order, const CSoundFile &sndFile) noexcept
{
	const auto endPat = order.begin() + order.GetLengthTailTrimmed();
	return std::find_if(order.begin(), endPat, [&](PATTERNINDEX pat) { return pat != PATTERNINDEX_SKIP && !sndFile.Patterns.IsValidPat(pat); }) != endPat;
}


bool RearrangeSequences(CSoundFile &sndFile, const std::vector<SEQUENCEINDEX> &newOrder)
{
	if(newOrder.empty() || newOrder.size() > MAX_SEQUENCES)
		return false;

	ModSequenceSet &sequences = sndFile.Order;
	const SEQUENCEINDEX oldCurrent = sequences.GetCurrentSequenceIndex();
	const std::vector<ModSequence> oldSequences(sequences.begin(), sequences.end());

	RemoveSequencesAfterFirst(sequences);
	while(sequences.GetNumSequences() < newOrder.size())
	{
		if(sequences.AddSequence() == SEQUENCEINDEX_INVALID)
			return false;
	}
	for(size_t i = 0; i < newOrder.size(); ++i)
	{
		sequences(static_cast<SEQUENCEINDEX>(i)) = (newOrder[i] < oldSequences.size()) ? oldSequences[newOrder[i]] : ModSequence{sndFile};
	}

	sequences.SetSequence(std::min(oldCurrent, static_cast<SEQUENCEINDEX>(sequences.GetNumSequences() - 1)));
	return true;
}


void OnSequencesModTypeChanged(CSoundFile &sndFile, MODTYPE oldType)
{
	for(auto &seq : sndFile.Order)
	{
		seq.AdjustToNewModType(oldType);
	}
	if(sndFile.GetModSpecifications(oldType).sequencesMax > 1 && sndFile.GetModSpecifications().sequencesMax <= 1)
		MergeSequences(sndFile);
}


bool CanSplitSubsongs(const CSoundFile &sndFile) noexcept
{
	return sndFile.Order.GetNumSequences() == 1 && sndFile.GetModSpecifications().sequencesMax > 1 && HasSubsongs(sndFile.Order(0), sndFile);
}


bool SplitSubsongsToMultipleSequences(CSoundFile &sndFile)
{
	if(!CanSplitSubsongs(sndFile))
		return false;

	ModSequenceSet &sequences = sndFile.Order;
	bool isModified = false;
	const ORDERINDEX length = sequences(0).GetLengthTailTrimmed();

	for(ORDERINDEX orderIndex = 0; orderIndex < length; ++orderIndex)
	{
		ModSequence &firstSeq = sequences(0);
		if(firstSeq.IsValidPat(orderIndex) || firstSeq[orderIndex] == PATTERNINDEX_SKIP)
			continue;

		// Remove all separator patterns between current and next subsong first
		while(orderIndex < length && !sndFile.Patterns.IsValidPat(sequences(0)[orderIndex]))
		{
			sequences(0)[orderIndex] = PATTERNINDEX_INVALID;
			++orderIndex;
			isModified = true;
		}
		if(orderIndex >= length)
			break;

		const SEQUENCEINDEX newSeq = sequences.AddSequence();
		if(newSeq == SEQUENCEINDEX_INVALID)
			break;

		const ORDERINDEX startOrd = orderIndex;
		sequences(newSeq).reserve(length - startOrd);
		isModified = true;

		while(orderIndex < length && sequences(0)[orderIndex] != PATTERNINDEX_INVALID)
		{
			const PATTERNINDEX copyPat = sequences(0)[orderIndex];
			sequences(newSeq).push_back(copyPat);
			sequences(0)[orderIndex] = PATTERNINDEX_INVALID;
			++orderIndex;

			if(sndFile.Patterns.IsValidPat(copyPat))
			{
				for(auto &m : sndFile.Patterns[copyPat])
				{
					if(m.command == CMD_POSITIONJUMP && m.param >= startOrd)
						m.param = static_cast<ModCommand::PARAM>(m.param - startOrd);
				}
			}
		}
		--orderIndex;
	}
	sequences.SetSequence(0);
	return isModified;
}


bool WriteGlobalsToPattern(CSoundFile &sndFile, SEQUENCEINDEX sequenceIndex, bool isWritingRestartPos, bool isWritingTempo)
{
	bool isSuccess = true;
	const auto subSongs = sndFile.GetLength(eNoAdjust, GetLengthTarget(true).StartPos(sequenceIndex, 0, 0));
	ModSequence &order = sndFile.Order(sequenceIndex);
	for(const auto &subSong : subSongs)
	{
		if(isWritingRestartPos && subSong.endOrder != ORDERINDEX_INVALID && subSong.endRow != ROWINDEX_INVALID)
		{
			if(mpt::in_range<ModCommand::PARAM>(order.GetRestartPos()))
			{
				const PATTERNINDEX writePat = order.EnsureUnique(subSong.endOrder);
				isSuccess &= sndFile.Patterns[writePat].WriteEffect(
					EffectWriter(CMD_POSITIONJUMP, static_cast<ModCommand::PARAM>(order.GetRestartPos())).Row(subSong.endRow).RetryNextRow());
			} else
			{
				isSuccess = false;
			}
		}
		if(isWritingTempo && subSong.startOrder != ORDERINDEX_INVALID && subSong.startRow != ORDERINDEX_INVALID)
		{
			const PATTERNINDEX writePat = order.EnsureUnique(subSong.startOrder);
			isSuccess &= sndFile.Patterns[writePat].WriteEffect(
				EffectWriter(CMD_TEMPO, mpt::saturate_round<ModCommand::PARAM>(order.GetDefaultTempo().ToDouble())).Row(subSong.startRow).RetryNextRow());
			isSuccess &= sndFile.Patterns[writePat].WriteEffect(
				EffectWriter(CMD_SPEED, mpt::saturate_cast<ModCommand::PARAM>(order.GetDefaultSpeed())).Row(subSong.startRow).RetryNextRow());
		}
	}
	order.SetRestartPos(0);
	return isSuccess;
}


bool MergeSequences(CSoundFile &sndFile)
{
	ModSequenceSet &sequences = sndFile.Order;
	if(sequences.GetNumSequences() <= 1)
		return false;

	ModSequence &firstSeq = sequences(0);
	firstSeq.resize(firstSeq.GetLengthTailTrimmed());
	// Which sequence already fixed the jump commands of each pattern
	std::vector<SEQUENCEINDEX> patternsFixed(sndFile.Patterns.Size(), SEQUENCEINDEX_INVALID);
	for(const auto pat : firstSeq)
	{
		if(sndFile.Patterns.IsValidPat(pat))
			patternsFixed[pat] = 0;
	}

	for(SEQUENCEINDEX sequenceNum = 1; sequenceNum < sequences.GetNumSequences(); ++sequenceNum)
	{
		ModSequence &sourceSeq = sequences(sequenceNum);
		const ORDERINDEX firstOrder = firstSeq.GetLength() + 1;  // +1 for separator item
		const ORDERINDEX lengthTrimmed = sourceSeq.GetLengthTailTrimmed();
		if(firstOrder + lengthTrimmed > sndFile.GetModSpecifications().ordersMax)
		{
			sndFile.AddToLog(LogWarning, MPT_UFORMAT("WARNING: Cannot merge Sequence {} (too long!)")(sequenceNum + 1));
			continue;
		}
		firstSeq.reserve(firstOrder + lengthTrimmed);
		firstSeq.push_back();  // Separator item
		WriteGlobalsToPattern(sndFile, sequenceNum, true, sourceSeq.GetDefaultTempo() != firstSeq.GetDefaultTempo() || sourceSeq.GetDefaultSpeed() != firstSeq.GetDefaultSpeed());
		patternsFixed.resize(sndFile.Patterns.Size(), SEQUENCEINDEX_INVALID);
		for(ORDERINDEX orderIndex = 0; orderIndex < lengthTrimmed; ++orderIndex)
		{
			PATTERNINDEX pat = sourceSeq[orderIndex];
			firstSeq.push_back(pat);

			if(!sndFile.Patterns.IsValidPat(pat))
				continue;

			auto m = sndFile.Patterns[pat].begin();
			const size_t numCommands = sndFile.Patterns[pat].GetNumRows() * sndFile.GetNumChannels();
			for(size_t len = 0; len < numCommands; ++m, ++len)
			{
				if(m->command != CMD_POSITIONJUMP)
					continue;
				if(patternsFixed[pat] != SEQUENCEINDEX_INVALID && patternsFixed[pat] != sequenceNum)
				{
					// Another sequence uses this pattern already
					const PATTERNINDEX newPat = sndFile.Patterns.Duplicate(pat, true);
					if(newPat != PATTERNINDEX_INVALID)
					{
						firstSeq[firstOrder + orderIndex] = newPat;
						m = sndFile.Patterns[newPat].begin() + len;
						if(newPat >= patternsFixed.size())
							patternsFixed.resize(newPat + 1, SEQUENCEINDEX_INVALID);
						pat = newPat;
					} else
					{
						sndFile.AddToLog(LogWarning, MPT_UFORMAT("CONFLICT: Pattern break commands in Pattern {} might be broken since it has been used in several sequences!")(pat));
					}
				}
				m->param = static_cast<ModCommand::PARAM>(m->param + firstOrder);
				patternsFixed[pat] = sequenceNum;
			}
		}
	}
	RemoveSequencesAfterFirst(sequences);
	sequences(0).SetName({});
	return true;
}


bool ExpandPattern(CPattern &pattern)
{
	const ROWINDEX oldRows = pattern.GetNumRows();
	if(!pattern.IsValid() || oldRows * 2 > pattern.GetSoundFile().GetModSpecifications().patternRowsMax)
		return false;
	if(!pattern.Resize(oldRows * 2, true, true))
		return false;

	for(ROWINDEX rowIndex = oldRows; rowIndex-- > 0;)
	{
		const auto src = pattern.GetRow(rowIndex);
		std::copy(src.begin(), src.end(), pattern.GetRow(rowIndex * 2).begin());
	}
	for(ROWINDEX rowIndex = 1; rowIndex < oldRows * 2; rowIndex += 2)
	{
		const auto dst = pattern.GetRow(rowIndex);
		std::fill(dst.begin(), dst.end(), ModCommand{});
	}
	return true;
}


bool ShrinkPattern(CPattern &pattern)
{
	if(!pattern.IsValid() || pattern.GetNumRows() < pattern.GetSoundFile().GetModSpecifications().patternRowsMin * 2)
		return false;

	const ROWINDEX newRows = pattern.GetNumRows() / 2;
	const CHANNELINDEX numChannels = pattern.GetNumChannels();
	for(ROWINDEX rowIndex = 0; rowIndex < newRows; ++rowIndex)
	{
		const auto srcRow = pattern.GetRow(rowIndex * 2);
		const auto nextSrcRow = pattern.GetRow(rowIndex * 2 + 1);
		const auto destRow = pattern.GetRow(rowIndex);

		for(CHANNELINDEX channelIndex = 0; channelIndex < numChannels; ++channelIndex)
		{
			const ModCommand src = srcRow[channelIndex];
			const ModCommand &srcNext = nextSrcRow[channelIndex];
			ModCommand &dest = destRow[channelIndex];
			dest = src;

			if(dest.note == NOTE_NONE && !dest.instr)
			{
				// Fill in data from next row if field is empty
				dest.note = srcNext.note;
				dest.instr = srcNext.instr;
				if(srcNext.volcmd != VOLCMD_NONE)
				{
					dest.volcmd = srcNext.volcmd;
					dest.vol = srcNext.vol;
				}
				if(dest.command == CMD_NONE)
				{
					dest.command = srcNext.command;
					dest.param = srcNext.param;
				}
			}
		}
	}
	return pattern.Resize(newRows, false, true);
}

OPENMPT_NAMESPACE_END
