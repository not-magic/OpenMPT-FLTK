/*
 * TuningExt.cpp
 * -------------
 * Purpose: Tuning export functions that upstream only compiles into the tracker build.
 * Notes  : Ported from upstream's tuning.cpp and tuningCollection.cpp, using only the public CTuning API.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "TuningExt.h"

#include "TrackerSettings.h"
#include "../soundlib/tuning.h"
#include "../soundlib/tuningcollection.h"
#include "mpt/io/io.hpp"
#include "mpt/io/io_stdstream.hpp"
#include "mpt/io_file/outputfile.hpp"

#include <cmath>

OPENMPT_NAMESPACE_BEGIN

namespace Tuning
{

namespace
{

void WriteSCLNote(std::ostream &f, double ratio, const std::string &noteName)
{
	mpt::IO::WriteTextCRLF(f, MPT_AFORMAT(" {} ! {}")(mpt::afmt::fix(std::log2(ratio) * 1200.0), noteName));
}

}  // namespace


bool WriteSCL(const CTuning &tuning, std::ostream &f, const mpt::PathString &filename)
{
	mpt::IO::WriteTextCRLF(f, MPT_AFORMAT("! {}")(mpt::ToCharset(mpt::Charset::ISO8859_1, (filename.GetFilenameBase() + filename.GetFilenameExtension()).ToUnicode())));
	mpt::IO::WriteTextCRLF(f, "!");
	std::string name = mpt::ToCharset(mpt::Charset::ISO8859_1, tuning.GetName());
	for(auto &c : name)
	{
		if(static_cast<uint8>(c) < 32)
			c = ' ';
	}
	// Do not confuse the description with a comment
	if(name.length() >= 1 && name[0] == '!')
		name[0] = '?';
	mpt::IO::WriteTextCRLF(f, name);

	const auto groupSize = tuning.GetGroupSize();
	const auto noteName = [&tuning](NOTEINDEXTYPE note) { return mpt::ToCharset(mpt::Charset::ISO8859_1, tuning.GetNoteName(note, false)); };
	switch(tuning.GetType())
	{
	case Type::GEOMETRIC:
		mpt::IO::WriteTextCRLF(f, MPT_AFORMAT(" {}")(groupSize));
		mpt::IO::WriteTextCRLF(f, "!");
		for(NOTEINDEXTYPE n = 0; n < static_cast<NOTEINDEXTYPE>(groupSize); ++n)
		{
			const double ratio = std::pow(static_cast<double>(tuning.GetGroupRatio()), static_cast<double>(n + 1) / static_cast<double>(groupSize));
			WriteSCLNote(f, ratio, noteName(static_cast<NOTEINDEXTYPE>((n + 1) % groupSize)));
		}
		return true;
	case Type::GROUPGEOMETRIC:
	{
		mpt::IO::WriteTextCRLF(f, MPT_AFORMAT(" {}")(groupSize));
		mpt::IO::WriteTextCRLF(f, "!");
		const double baseRatio = static_cast<double>(tuning.GetRatio(0));
		for(NOTEINDEXTYPE n = 0; n < static_cast<NOTEINDEXTYPE>(groupSize); ++n)
		{
			const bool isLast = (n == static_cast<NOTEINDEXTYPE>(groupSize - 1));
			const double ratio = static_cast<double>(isLast ? tuning.GetGroupRatio() : tuning.GetRatio(n + 1)) / baseRatio;
			WriteSCLNote(f, ratio, noteName(static_cast<NOTEINDEXTYPE>((n + 1) % groupSize)));
		}
		return true;
	}
	case Type::GENERAL:
	{
		const auto noteRange = tuning.GetNoteRange();
		const NOTEINDEXTYPE numNotes = static_cast<NOTEINDEXTYPE>(noteRange.last - noteRange.first + 1);
		mpt::IO::WriteTextCRLF(f, MPT_AFORMAT(" {}")(numNotes + 1));
		mpt::IO::WriteTextCRLF(f, "!");
		double baseRatio = 1.0;
		for(NOTEINDEXTYPE n = 0; n < numNotes; ++n)
		{
			baseRatio = std::min(baseRatio, static_cast<double>(tuning.GetRatio(noteRange.first + n)));
		}
		for(NOTEINDEXTYPE n = 0; n < numNotes; ++n)
		{
			const NOTEINDEXTYPE note = noteRange.first + n;
			WriteSCLNote(f, static_cast<double>(tuning.GetRatio(note)) / baseRatio, noteName(note));
		}
		mpt::IO::WriteTextCRLF(f, MPT_AFORMAT(" {} ! {}")(mpt::afmt::val(1), std::string()));
		return true;
	}
	}
	return false;
}


bool UnpackTuningCollection(const CTuningCollection &tc, const mpt::PathString &prefix)
{
	bool hasError = false;
	const auto numberFmt = mpt::format_simple_spec<mpt::ustring>().Dec().FillNul().Width(1 + static_cast<int>(std::log10(tc.GetNumTunings())));
	for(std::size_t i = 0; i < tc.GetNumTunings(); ++i)
	{
		const CTuning &tuning = *(tc.GetTuning(i));
		mpt::ustring tuningName = tuning.GetName();
		if(tuningName.empty())
			tuningName = U_("untitled");
		mpt::PathString fileName = prefix;
		fileName += mpt::PathString::FromUnicode(MPT_UFORMAT("{} - {}")(mpt::ufmt::fmt(i + 1, numberFmt), tuningName)).AsSanitizedComponent();
		fileName += mpt::PathString::FromUTF8(CTuning::s_FileExtension);
		if(FileSystem::Exists(fileName))
		{
			hasError = true;
			continue;
		}
		mpt::IO::SafeOutputFile sfout(fileName, std::ios::binary, mpt::IO::FlushModeFromBool(TrackerSettings::Instance().MiscFlushFileBuffersOnSave));
		if(tuning.Serialize(sfout) != SerializationResult::Success)
			hasError = true;
	}
	return !hasError;
}

}  // namespace Tuning

OPENMPT_NAMESPACE_END
