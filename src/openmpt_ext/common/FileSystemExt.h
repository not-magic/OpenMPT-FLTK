/*
 * FileSystemExt.h
 * ---------------
 * Purpose: File system queries and well-known directories for the tracker, on every platform.
 * Notes  : Upstream's mpt::native_fs and mpt::common_directories only exist on Windows.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../common/mptPathString.h"

#include <filesystem>

OPENMPT_NAMESPACE_BEGIN

namespace FileSystem
{

std::filesystem::path ToFilesystemPath(const mpt::PathString &path);
mpt::PathString FromFilesystemPath(const std::filesystem::path &path);

bool IsDirectory(const mpt::PathString &path);
bool IsFile(const mpt::PathString &path);
bool Exists(const mpt::PathString &path);

// These return an empty path if the directory cannot be determined, otherwise a path with a trailing separator
mpt::PathString FindApplicationDirectory();
mpt::PathString FindTempDirectory();
mpt::PathString FindHomeDirectory();
// Per-user application data: %APPDATA% on Windows, ~/Library/Application Support on macOS, $XDG_CONFIG_HOME or ~/.config elsewhere
mpt::PathString FindConfigDirectory();

}  // namespace FileSystem

OPENMPT_NAMESPACE_END
