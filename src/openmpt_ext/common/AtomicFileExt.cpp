/*
 * AtomicFileExt.cpp
 * -----------------
 * Purpose: File shared between running instances, read and written whole under an inter-process lock.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


#include "stdafx.h"
#include "AtomicFileExt.h"

#include <algorithm>
#include <stdexcept>

#if MPT_OS_WINDOWS
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

OPENMPT_NAMESPACE_BEGIN

#if MPT_OS_WINDOWS

namespace
{

enum class LockMode
{
	Shared,
	Exclusive,
};

void LockWholeFile(void *handle, LockMode mode)
{
	OVERLAPPED overlapped{};
	const DWORD flags = (mode == LockMode::Exclusive) ? LOCKFILE_EXCLUSIVE_LOCK : 0;
	if(!::LockFileEx(static_cast<HANDLE>(handle), flags, 0, MAXDWORD, MAXDWORD, &overlapped))
		throw std::runtime_error("Cannot lock file");
}

}  // namespace


AtomicSharedFile::AtomicSharedFile(const mpt::PathString &filename)
{
	const std::wstring path = filename.AsNative();
	HANDLE handle = ::CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if(handle == INVALID_HANDLE_VALUE)
	{
		m_isReadOnly = true;
		handle = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	}
	if(handle == INVALID_HANDLE_VALUE)
		throw std::runtime_error("Cannot open file");
	m_handle = handle;
}


AtomicSharedFile::~AtomicSharedFile()
{
	::CloseHandle(static_cast<HANDLE>(m_handle));
}


void AtomicSharedFile::lock()
{
	if(m_lockCount++ == 0)
		LockWholeFile(m_handle, LockMode::Exclusive);
}


void AtomicSharedFile::lock_shared()
{
	if(m_lockCount++ == 0)
		LockWholeFile(m_handle, LockMode::Shared);
}


void AtomicSharedFile::unlock()
{
	if(--m_lockCount == 0)
	{
		OVERLAPPED overlapped{};
		::UnlockFileEx(static_cast<HANDLE>(m_handle), 0, MAXDWORD, MAXDWORD, &overlapped);
	}
}


std::vector<std::byte> AtomicSharedFile::Read()
{
	const HANDLE handle = static_cast<HANDLE>(m_handle);
	LARGE_INTEGER size{};
	if(!::GetFileSizeEx(handle, &size))
		throw std::runtime_error("Cannot read file");
	std::vector<std::byte> data(static_cast<std::size_t>(size.QuadPart));
	LARGE_INTEGER start{};
	::SetFilePointerEx(handle, start, nullptr, FILE_BEGIN);
	std::size_t bytesRead = 0;
	while(bytesRead < data.size())
	{
		DWORD chunkSize = 0;
		const DWORD requestSize = static_cast<DWORD>(std::min<std::size_t>(data.size() - bytesRead, MAXDWORD));
		if(!::ReadFile(handle, data.data() + bytesRead, requestSize, &chunkSize, nullptr) || chunkSize == 0)
			throw std::runtime_error("Cannot read file");
		bytesRead += chunkSize;
	}
	return data;
}


bool AtomicSharedFile::Write(mpt::const_byte_span data, bool isSyncRequested)
{
	if(m_isReadOnly)
		return false;
	const HANDLE handle = static_cast<HANDLE>(m_handle);
	LARGE_INTEGER start{};
	if(!::SetFilePointerEx(handle, start, nullptr, FILE_BEGIN) || !::SetEndOfFile(handle))
		throw std::runtime_error("Cannot write file");
	std::size_t bytesWritten = 0;
	while(bytesWritten < data.size())
	{
		DWORD chunkSize = 0;
		const DWORD requestSize = static_cast<DWORD>(std::min<std::size_t>(data.size() - bytesWritten, MAXDWORD));
		if(!::WriteFile(handle, data.data() + bytesWritten, requestSize, &chunkSize, nullptr) || chunkSize == 0)
			throw std::runtime_error("Cannot write file");
		bytesWritten += chunkSize;
	}
	if(isSyncRequested)
		::FlushFileBuffers(handle);
	return true;
}

#else  // !MPT_OS_WINDOWS

AtomicSharedFile::AtomicSharedFile(const mpt::PathString &filename)
{
	const auto path = filename.AsNative();
	m_fileDescriptor = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0666);
	if(m_fileDescriptor < 0)
	{
		m_isReadOnly = true;
		m_fileDescriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
	}
	if(m_fileDescriptor < 0)
		throw std::runtime_error("Cannot open file");
}


AtomicSharedFile::~AtomicSharedFile()
{
	::close(m_fileDescriptor);
}


void AtomicSharedFile::lock()
{
	if(m_lockCount++ == 0)
		::flock(m_fileDescriptor, LOCK_EX);
}


void AtomicSharedFile::lock_shared()
{
	if(m_lockCount++ == 0)
		::flock(m_fileDescriptor, LOCK_SH);
}


void AtomicSharedFile::unlock()
{
	if(--m_lockCount == 0)
		::flock(m_fileDescriptor, LOCK_UN);
}


std::vector<std::byte> AtomicSharedFile::Read()
{
	struct stat info{};
	if(::fstat(m_fileDescriptor, &info) != 0)
		throw std::runtime_error("Cannot read file");
	std::vector<std::byte> data(static_cast<std::size_t>(info.st_size));
	std::size_t bytesRead = 0;
	while(bytesRead < data.size())
	{
		const ssize_t chunkSize = ::pread(m_fileDescriptor, data.data() + bytesRead, data.size() - bytesRead, static_cast<off_t>(bytesRead));
		if(chunkSize <= 0)
			throw std::runtime_error("Cannot read file");
		bytesRead += static_cast<std::size_t>(chunkSize);
	}
	return data;
}


bool AtomicSharedFile::Write(mpt::const_byte_span data, bool isSyncRequested)
{
	if(m_isReadOnly)
		return false;
	if(::ftruncate(m_fileDescriptor, 0) != 0)
		throw std::runtime_error("Cannot write file");
	std::size_t bytesWritten = 0;
	while(bytesWritten < data.size())
	{
		const ssize_t chunkSize = ::pwrite(m_fileDescriptor, data.data() + bytesWritten, data.size() - bytesWritten, static_cast<off_t>(bytesWritten));
		if(chunkSize <= 0)
			throw std::runtime_error("Cannot write file");
		bytesWritten += static_cast<std::size_t>(chunkSize);
	}
	if(isSyncRequested)
		::fsync(m_fileDescriptor);
	return true;
}

#endif  // MPT_OS_WINDOWS


void AtomicSharedFile::unlock_shared()
{
	unlock();
}

OPENMPT_NAMESPACE_END
