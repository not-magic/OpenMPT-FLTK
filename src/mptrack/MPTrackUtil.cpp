/*
 * MPTrackUtil.cpp
 * ---------------
 * Purpose: Various useful utility functions.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "ui/Ui.h"
#include "MPTrackUtil.h"

#include <filesystem>

#include <fnmatch.h>
#include <spawn.h>
#include <sys/wait.h>

extern char **environ;
#include "ui/ResourceTables.h"


OPENMPT_NAMESPACE_BEGIN


mpt::const_byte_span GetResource(uint32 resourceId)
{
	const ui::BinaryResource *resource = ui::FindBinaryResource(resourceId);
	if(resource == nullptr)
		return mpt::const_byte_span();
	return mpt::const_byte_span(reinterpret_cast<const std::byte *>(resource->data), resource->size);
}


mpt::ustring LoadResourceString(uint32 nID)
{
	const char *text = ui::FindStringResource(nID);
	MPT_ASSERT(text);
	if(text == nullptr)
		return mpt::ustring();
	return mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, std::string(text));
}


namespace Util
{

uint64 GetTickCount64()
{
	return static_cast<uint64>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

}  // namespace Util


namespace Util
{
	mpt::ustring FormatLocalTime(int64 unixSeconds, const char *format)
	{
		const std::time_t time = static_cast<std::time_t>(unixSeconds);
		std::tm localTime{};
		localtime_r(&time, &localTime);
		std::array<char, 128> buffer{};
		const std::size_t length = std::strftime(buffer.data(), buffer.size(), format, &localTime);
		return mpt::transcode<mpt::ustring>(mpt::common_encoding::utf8, std::string(buffer.data(), length));
	}

	bool DeleteFile(const mpt::PathString &path)
	{
		std::error_code ec;
		return std::filesystem::remove(std::filesystem::path(path.AsNative()), ec);
	}

	bool MoveFile(const mpt::PathString &from, const mpt::PathString &to)
	{
		std::error_code ec;
		std::filesystem::rename(std::filesystem::path(from.AsNative()), std::filesystem::path(to.AsNative()), ec);
		return !ec;
	}

	bool CopyFile(const mpt::PathString &from, const mpt::PathString &to, bool isOverwriting)
	{
		std::error_code ec;
		return std::filesystem::copy_file(std::filesystem::path(from.AsNative()), std::filesystem::path(to.AsNative()),
			isOverwriting ? std::filesystem::copy_options::overwrite_existing : std::filesystem::copy_options::none, ec);
	}

	bool CreateDirectory(const mpt::PathString &path)
	{
		std::error_code ec;
		return std::filesystem::create_directories(std::filesystem::path(path.AsNative()), ec);
	}

	bool RemoveDirectory(const mpt::PathString &path)
	{
		std::error_code ec;
		return std::filesystem::remove(std::filesystem::path(path.AsNative()), ec);
	}

	bool MoveToTrash(const mpt::PathString &path)
	{
		const std::string nativePath = mpt::transcode<std::string>(mpt::common_encoding::utf8, path.ToUnicode());
		const char *const arguments[] = {"gio", "trash", "--", nativePath.c_str(), nullptr};
		pid_t pid = 0;
		if(posix_spawnp(&pid, "gio", nullptr, nullptr, const_cast<char *const *>(arguments), environ) != 0)
			return false;
		int status = 0;
		if(waitpid(pid, &status, 0) < 0)
			return false;
		return WIFEXITED(status) && WEXITSTATUS(status) == 0;
	}

	bool IsNaturalLess(const mpt::ustring &left, const mpt::ustring &right)
	{
		const auto isDigit = [](mpt::uchar c) { return c >= UC_('0') && c <= UC_('9'); };
		const auto lower = [](mpt::uchar c) { return (c >= UC_('A') && c <= UC_('Z')) ? static_cast<mpt::uchar>(c - UC_('A') + UC_('a')) : c; };
		std::size_t l = 0, r = 0;
		while(l < left.size() && r < right.size())
		{
			if(isDigit(left[l]) && isDigit(right[r]))
			{
				std::size_t lEnd = l, rEnd = r;
				while(lEnd < left.size() && isDigit(left[lEnd]))
					++lEnd;
				while(rEnd < right.size() && isDigit(right[rEnd]))
					++rEnd;
				while(l < lEnd - 1 && left[l] == UC_('0'))
					++l;
				while(r < rEnd - 1 && right[r] == UC_('0'))
					++r;
				if(lEnd - l != rEnd - r)
					return lEnd - l < rEnd - r;
				for(; l < lEnd; ++l, ++r)
				{
					if(left[l] != right[r])
						return left[l] < right[r];
				}
				continue;
			}
			const mpt::uchar lc = lower(left[l]), rc = lower(right[r]);
			if(lc != rc)
				return lc < rc;
			++l;
			++r;
		}
		return (left.size() - l) < (right.size() - r);
	}

	bool IsWildcardMatch(const mpt::ustring &pattern, const mpt::ustring &name)
	{
		const std::string patternUtf8 = mpt::transcode<std::string>(mpt::common_encoding::utf8, pattern);
		const std::string nameUtf8 = mpt::transcode<std::string>(mpt::common_encoding::utf8, name);
		return fnmatch(patternUtf8.c_str(), nameUtf8.c_str(), FNM_CASEFOLD) == 0;
	}
}


OPENMPT_NAMESPACE_END
