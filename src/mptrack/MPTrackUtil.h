/*
 * MPTrackUtil.h
 * -------------
 * Purpose: Various useful utility functions.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */
// This file was translated by AI and may not represent the original authors' intent.

// FLTK port of openmpt/mptrack/MPTrackUtil.h

#pragma once

#include "openmpt/all/BuildSettings.hpp"
#include "ui/Ui.h"


#include <string>


OPENMPT_NAMESPACE_BEGIN


/*
 * Gets an embedded resource as raw byte data.
 * [in] resourceId: ID from resource.h
 * Return: span representing the resource data, empty if there is no such resource.
 */
mpt::const_byte_span GetResource(uint32 resourceId);


mpt::ustring LoadResourceString(uint32 nID);


namespace Util
{
	// Milliseconds since an arbitrary point in the past, never goes backwards
	uint64 GetTickCount64();

	// Formats a Unix time stamp in the local time zone using strftime format codes
	mpt::ustring FormatLocalTime(int64 unixSeconds, const char *format);

	bool DeleteFile(const mpt::PathString &path);
	bool MoveFile(const mpt::PathString &from, const mpt::PathString &to);
	bool CopyFile(const mpt::PathString &from, const mpt::PathString &to, bool isOverwriting = true);
	// Creates the directory and all missing parent directories
	bool CreateDirectory(const mpt::PathString &path);
	bool RemoveDirectory(const mpt::PathString &path);
	// Moves a file to the desktop trash; returns false if that is not possible
	bool MoveToTrash(const mpt::PathString &path);

	// Case-insensitive order in which runs of digits are compared by value
	bool IsNaturalLess(const mpt::ustring &left, const mpt::ustring &right);
	// Case-insensitive match of a name against a pattern with * and ? wildcards
	bool IsWildcardMatch(const mpt::ustring &pattern, const mpt::ustring &name);
}


OPENMPT_NAMESPACE_END
