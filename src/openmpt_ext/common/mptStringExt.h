/*
 * mptStringExt.h
 * --------------
 * Purpose: Tracker-only string, path and logging helpers that libopenmpt does not build.
 * Notes  : Declared in the same namespaces as upstream so tracker code can call them unchanged.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../common/mptPathString.h"
#include "../common/mptString.h"
#include "mpt/string_transcode/transcode.hpp"

OPENMPT_NAMESPACE_BEGIN

namespace mpt
{

mpt::ustring ToLowerCaseLocale(mpt::ustring_view s);
mpt::ustring ToUpperCaseLocale(mpt::ustring_view s);

mpt::PathString AbsolutePathToRelative(const mpt::PathString &path, const mpt::PathString &relativeTo);
mpt::PathString RelativePathToAbsolute(const mpt::PathString &path, const mpt::PathString &relativeTo);

int PathCompareNoCase(const PathString &a, const PathString &b);

template <typename Tstring>
inline Tstring SanitizePathComponent(const Tstring &str)
{
	return mpt::transcode<Tstring>(mpt::native_path::FromNative(mpt::transcode<mpt::native_path::raw_path_type>(str)).AsSanitizedComponent().AsNative());
}

namespace log
{
namespace Trace
{

enum ThreadKind
{
	ThreadKindGUI,
	ThreadKindAudio,
	ThreadKindNotify,
	ThreadKindWatchdir,
};

uint32 GetCurrentThreadId() noexcept;

void SetThreadId(ThreadKind kind, uint32 id);
uint32 GetThreadId(ThreadKind kind);

}  // namespace Trace
}  // namespace log

}  // namespace mpt

OPENMPT_NAMESPACE_END

namespace mpt
{
inline namespace MPT_INLINE_NS
{

// The tracker build has no OpenMPT namespace, so mpt::ToUString(PathString) is found by ADL there
template <typename Tstring, typename T, std::enable_if_t<std::is_same_v<T, OPENMPT_NAMESPACE::mpt::PathString>, bool> = true>
inline Tstring format_value_default(const T &path)
{
	return mpt::transcode<Tstring>(path.ToUnicode());
}

}  // namespace MPT_INLINE_NS
}  // namespace mpt
