/*
 * FolderScanner.h
 * ---------------
 * Purpose: Class for easily scanning a folder (and its subfolders) for files and directories.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"

#include "../common/mptPathString.h"

#include <chrono>
#include <filesystem>
#include <vector>

OPENMPT_NAMESPACE_BEGIN

struct FolderEntryInfo
{
	std::filesystem::file_time_type lastWriteTime;
	uint64 size = 0;
	bool isDirectory = false;
};

// Matches a file name against a list of wildcard patterns separated by ';' ('*' and '?' are supported, case-insensitive).
bool MatchFileSpec(const mpt::ustring &name, const mpt::ustring &spec);

class FolderScanner
{
public:
	enum ScanType
	{
		kOnlyFiles = 0x01,
		kOnlyDirectories = 0x02,
		kFilesAndDirectories = kOnlyFiles | kOnlyDirectories,
		kFindInSubDirectories = 0x04,
	};

	FolderScanner(mpt::PathString path, FlagSet<ScanType> type, mpt::PathString filter = P_("*.*"));

	FolderScanner(const FolderScanner&) = delete;
	FolderScanner &operator=(const FolderScanner&) = delete;

	// Return one file or directory at a time in parameter file. Returns true if a file was found (file parameter is valid), false if no more files can be found (file parameter is not touched).
	bool Next(mpt::PathString &file, FolderEntryInfo *fileInfo = nullptr);

protected:
	std::vector<mpt::PathString> m_paths;
	std::vector<std::filesystem::directory_entry> m_entries;
	std::size_t m_nextEntry = 0;
	mpt::PathString m_currentPath;
	const mpt::PathString m_filter;
	const FlagSet<ScanType> m_type;
};

MPT_DECLARE_ENUM(FolderScanner::ScanType)

OPENMPT_NAMESPACE_END
