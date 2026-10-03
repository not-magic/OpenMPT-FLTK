// OS-agnostic replacement for upstream's Windows-only mpt::native_fs and
// mpt::common_directories (openmpt/src/mpt/fs/).

#include "stdafx.h"
#include "FileSystemExt.h"

#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <vector>

#if MPT_OS_WINDOWS
#include <windows.h>
#elif MPT_OS_MACOSX_OR_IOS
#include <mach-o/dyld.h>
#endif

OPENMPT_NAMESPACE_BEGIN

namespace
{

mpt::PathString FindDirectory(const std::filesystem::path &path)
{
	if(path.empty())
		return mpt::PathString();
	return FileSystem::FromFilesystemPath(path).WithTrailingSlash();
}

std::filesystem::path FindEnvironmentPath(const char *name)
{
#if MPT_OS_WINDOWS
	const std::wstring wideName = mpt::transcode<std::wstring>(mpt::common_encoding::utf8, std::string(name));
	if(const wchar_t *value = _wgetenv(wideName.c_str()); value && *value)
		return std::filesystem::path(value);
#else
	if(const char *value = std::getenv(name); value && *value)
		return std::filesystem::path(value);
#endif
	return std::filesystem::path();
}

std::filesystem::path FindExecutablePath()
{
#if MPT_OS_WINDOWS
	std::vector<wchar_t> buffer(MAX_PATH);
	while(true)
	{
		const DWORD length = ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
		if(length == 0)
			return std::filesystem::path();
		if(length < buffer.size())
			return std::filesystem::path(std::wstring(buffer.data(), length));
		buffer.resize(buffer.size() * 2);
	}
#elif MPT_OS_MACOSX_OR_IOS
	uint32_t size = 0;
	_NSGetExecutablePath(nullptr, &size);
	std::vector<char> buffer(size + 1);
	if(_NSGetExecutablePath(buffer.data(), &size) != 0)
		return std::filesystem::path();
	std::error_code ec;
	return std::filesystem::canonical(std::filesystem::path(buffer.data()), ec);
#else
	std::error_code ec;
	return std::filesystem::canonical("/proc/self/exe", ec);
#endif
}

}  // namespace

namespace FileSystem
{

std::filesystem::path ToFilesystemPath(const mpt::PathString &path)
{
	return std::filesystem::path(path.AsNative().c_str());
}


mpt::PathString FromFilesystemPath(const std::filesystem::path &path)
{
	return mpt::PathString::FromNative(mpt::os_path(path.c_str()));
}


bool IsDirectory(const mpt::PathString &path)
{
	std::error_code ec;
	return std::filesystem::is_directory(ToFilesystemPath(path), ec);
}


bool IsFile(const mpt::PathString &path)
{
	std::error_code ec;
	const std::filesystem::file_status status = std::filesystem::status(ToFilesystemPath(path), ec);
	return std::filesystem::exists(status) && !std::filesystem::is_directory(status);
}


bool Exists(const mpt::PathString &path)
{
	std::error_code ec;
	return std::filesystem::exists(ToFilesystemPath(path), ec);
}


mpt::PathString FindApplicationDirectory()
{
	return FindDirectory(FindExecutablePath().parent_path());
}


mpt::PathString FindTempDirectory()
{
	std::error_code ec;
	const std::filesystem::path tempDir = std::filesystem::temp_directory_path(ec);
	return ec ? mpt::PathString() : FindDirectory(tempDir);
}


mpt::PathString FindHomeDirectory()
{
#if MPT_OS_WINDOWS
	return FindDirectory(FindEnvironmentPath("USERPROFILE"));
#else
	return FindDirectory(FindEnvironmentPath("HOME"));
#endif
}


mpt::PathString FindConfigDirectory()
{
#if MPT_OS_WINDOWS
	return FindDirectory(FindEnvironmentPath("APPDATA"));
#elif MPT_OS_MACOSX_OR_IOS
	const std::filesystem::path home = FindEnvironmentPath("HOME");
	return home.empty() ? mpt::PathString() : FindDirectory(home / "Library" / "Application Support");
#else
	if(const std::filesystem::path configHome = FindEnvironmentPath("XDG_CONFIG_HOME"); !configHome.empty())
		return FindDirectory(configHome);
	const std::filesystem::path home = FindEnvironmentPath("HOME");
	return home.empty() ? mpt::PathString() : FindDirectory(home / ".config");
#endif
}

}  // namespace FileSystem

OPENMPT_NAMESPACE_END
