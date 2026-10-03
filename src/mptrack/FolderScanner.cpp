// FLTK port of openmpt/mptrack/FolderScanner.cpp

#include "stdafx.h"
#include "ui/Ui.h"
#include "FolderScanner.h"

#include <system_error>

OPENMPT_NAMESPACE_BEGIN

namespace
{

bool MatchWildcard(std::string_view name, std::string_view pattern)
{
	std::size_t n = 0, p = 0;
	std::size_t starPattern = std::string_view::npos, starName = 0;
	while(n < name.size())
	{
		if(p < pattern.size() && (pattern[p] == '?' || pattern[p] == name[n]))
		{
			++n;
			++p;
		} else if(p < pattern.size() && pattern[p] == '*')
		{
			starPattern = p++;
			starName = n;
		} else if(starPattern != std::string_view::npos)
		{
			p = starPattern + 1;
			n = ++starName;
		} else
		{
			return false;
		}
	}
	while(p < pattern.size() && pattern[p] == '*')
		++p;
	return p == pattern.size();
}

std::string ToLowerUtf8(const mpt::ustring &str)
{
	std::string result = mpt::transcode<std::string>(mpt::common_encoding::utf8, mpt::ToLowerCaseLocale(str));
	return result;
}

}  // namespace


bool MatchFileSpec(const mpt::ustring &name, const mpt::ustring &spec)
{
	const std::string loweredName = ToLowerUtf8(name);
	const std::string loweredSpec = ToLowerUtf8(spec);
	std::size_t start = 0;
	while(start <= loweredSpec.size())
	{
		std::size_t end = loweredSpec.find(';', start);
		if(end == std::string::npos)
			end = loweredSpec.size();
		std::string_view pattern(loweredSpec.data() + start, end - start);
		while(!pattern.empty() && pattern.front() == ' ')
			pattern.remove_prefix(1);
		if(!pattern.empty() && MatchWildcard(loweredName, pattern))
			return true;
		start = end + 1;
	}
	return false;
}


FolderScanner::FolderScanner(mpt::PathString path, FlagSet<ScanType> type, mpt::PathString filter)
	: m_paths{1, std::move(path)}
	, m_filter{std::move(filter)}
	, m_type{type}
{
}


bool FolderScanner::Next(mpt::PathString &file, FolderEntryInfo *fileInfo)
{
	while(true)
	{
		while(m_nextEntry >= m_entries.size())
		{
			if(m_paths.empty())
				return false;
			m_currentPath = m_paths.back().WithTrailingSlash();
			m_paths.pop_back();
			m_entries.clear();
			m_nextEntry = 0;
			std::error_code ec;
			for(std::filesystem::directory_iterator it(std::filesystem::path(m_currentPath.AsNative().c_str()), ec); !ec && it != std::filesystem::directory_iterator(); it.increment(ec))
				m_entries.push_back(*it);
		}

		const std::filesystem::directory_entry &entry = m_entries[m_nextEntry++];
		std::error_code ec;
		const bool isDirectory = entry.is_directory(ec);
		const mpt::PathString entryPath = m_currentPath + mpt::PathString::FromNative(mpt::os_path(entry.path().filename().c_str()));
		bool isFound = false;
		if(isDirectory)
		{
			if(m_type[kFindInSubDirectories])
				m_paths.push_back(entryPath);
			isFound = m_type[kOnlyDirectories];
		} else
		{
			isFound = m_type[kOnlyFiles];
		}
		// Directories are matched against the filter as well, like the wildcard search of the file system would do
		if(isFound && !MatchFileSpec(entryPath.GetFilename().ToUnicode(), m_filter.ToUnicode()) && !(m_filter == P_("*.*")))
			isFound = false;
		if(!isFound)
			continue;
		file = entryPath;
		if(fileInfo)
		{
			fileInfo->isDirectory = isDirectory;
			fileInfo->size = isDirectory ? 0 : static_cast<uint64>(entry.file_size(ec));
			fileInfo->lastWriteTime = entry.last_write_time(ec);
		}
		return true;
	}
}

OPENMPT_NAMESPACE_END
