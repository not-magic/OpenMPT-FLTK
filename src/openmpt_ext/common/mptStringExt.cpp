// Tracker-only helpers from openmpt/common/mptString.h, mptPathString.cpp and Logging.cpp,
// which libopenmpt does not build.

#include "stdafx.h"
#include "mptStringExt.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cwctype>
#include <filesystem>


OPENMPT_NAMESPACE_BEGIN

namespace
{

template <typename Tfunc>
mpt::ustring TransformWide(mpt::ustring_view s, Tfunc func)
{
	std::wstring ws = mpt::transcode<std::wstring>(mpt::ustring(s));
	std::transform(ws.begin(), ws.end(), ws.begin(), func);
	return mpt::transcode<mpt::ustring>(ws);
}

std::array<std::atomic<uint32>, 4> g_traceThreadIds{};

template <typename Tchar>
bool IsPathSeparator(Tchar c)
{
	return c == '/' || c == '\\';
}

bool IsSamePath(const mpt::PathString &a, const mpt::PathString &b)
{
#if MPT_OS_WINDOWS || MPT_OS_MACOSX_OR_IOS
	return !mpt::PathCompareNoCase(a, b);
#else
	return a == b;
#endif
}

}  // namespace

namespace mpt
{

mpt::ustring ToLowerCaseLocale(mpt::ustring_view s)
{
	return TransformWide(s, [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
}


mpt::ustring ToUpperCaseLocale(mpt::ustring_view s)
{
	return TransformWide(s, [](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); });
}


mpt::PathString AbsolutePathToRelative(const mpt::PathString &path, const mpt::PathString &relativeTo)
{
	const mpt::os_path &pathNative = path.AsNative();
	const mpt::os_path baseNative = relativeTo.WithTrailingSlash().AsNative();
	if(pathNative.empty() || relativeTo.empty() || pathNative.length() < baseNative.length())
		return path;
	const mpt::PathString pathPrefix = mpt::PathString::FromNative(pathNative.substr(0, baseNative.length()));
	if(!IsSamePath(pathPrefix, mpt::PathString::FromNative(baseNative)))
		return path;
	// Path is in relativeTo or a sub directory ("/opt/OpenMPT/Somepath" => "./Somepath")
	mpt::os_path result = MPT_OS_PATH(".");
	result += static_cast<mpt::os_path::value_type>(std::filesystem::path::preferred_separator);
	result += pathNative.substr(baseNative.length());
	return mpt::PathString::FromNative(result);
}


mpt::PathString RelativePathToAbsolute(const mpt::PathString &path, const mpt::PathString &relativeTo)
{
	if(path.empty() || relativeTo.empty())
		return path;
	// Modules saved on another OS may use the other separator (".\Samples\x.flac")
	if(std::filesystem::path(path.AsNative().c_str()).is_absolute())
		return path;
	mpt::os_path pathNative = path.AsNative();
	std::replace_if(pathNative.begin(), pathNative.end(), [](auto c) { return IsPathSeparator(c); }, static_cast<mpt::os_path::value_type>(std::filesystem::path::preferred_separator));
	const mpt::os_path baseNative = relativeTo.WithTrailingSlash().AsNative();
	if(pathNative.length() >= 2 && pathNative[0] == '.' && IsPathSeparator(pathNative[1]))
		return mpt::PathString::FromNative(baseNative + pathNative.substr(2));
	return mpt::PathString::FromNative(baseNative + pathNative);
}


int PathCompareNoCase(const PathString &a, const PathString &b)
{
	return ToLowerCaseLocale(a.ToUnicode()).compare(ToLowerCaseLocale(b.ToUnicode()));
}


namespace log
{
namespace Trace
{

uint32 GetCurrentThreadId() noexcept
{
	// Only needs to tell the threads of this process apart
	static std::atomic<uint32> s_nextThreadId{1};
	thread_local const uint32 threadId = s_nextThreadId++;
	return threadId;
}


void SetThreadId(ThreadKind kind, uint32 id)
{
	if(id != 0)
		g_traceThreadIds[kind] = id;
}


uint32 GetThreadId(ThreadKind kind)
{
	return g_traceThreadIds[kind];
}

}  // namespace Trace
}  // namespace log

}  // namespace mpt

OPENMPT_NAMESPACE_END
