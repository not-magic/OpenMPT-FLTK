// Inter-process locked file for settings. Upstream's mpt::IO::atomic_shared_file_ref
// (openmpt/src/mpt/io_file_atomic/atomic_file.hpp) only exists in the Windows tracker build.

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "../common/mptPathString.h"
#include "mpt/base/span.hpp"

#include <cstddef>
#include <vector>

OPENMPT_NAMESPACE_BEGIN

class AtomicSharedFile
{
public:
	explicit AtomicSharedFile(const mpt::PathString &filename);
	~AtomicSharedFile();

	AtomicSharedFile(const AtomicSharedFile &) = delete;
	AtomicSharedFile &operator=(const AtomicSharedFile &) = delete;

	void lock();
	void lock_shared();
	void unlock();
	void unlock_shared();

	std::vector<std::byte> Read();
	// Replaces the file contents. Returns false if the file is read-only.
	bool Write(mpt::const_byte_span data, bool isSyncRequested);

private:
#if MPT_OS_WINDOWS
	void *m_handle = nullptr;
#else
	int m_fileDescriptor = -1;
#endif
	std::size_t m_lockCount = 0;
	bool m_isReadOnly = false;
};

OPENMPT_NAMESPACE_END
