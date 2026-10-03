// TemporaryPathname from openmpt/common/mptFileTemporary.h, which only exists in the Windows tracker build.

#pragma once

#include "openmpt/all/BuildSettings.hpp"

#include "FileSystemExt.h"
#include "../common/mptPathString.h"
#include "../common/mptRandom.h"
#include "mpt/io_file_unique/unique_basename.hpp"
#include "mpt/uuid/uuid.hpp"

OPENMPT_NAMESPACE_BEGIN

namespace mpt
{

class TemporaryPathname
{
public:
	TemporaryPathname(const mpt::PathString &fileNameExtension = P_("tmp"))
	{
		const mpt::IO::unique_basename basename{MPT_OS_PATH("OpenMPT"), mpt::UUID::GenerateLocalUseOnly(mpt::global_prng())};
		m_path = FileSystem::FindTempDirectory() + mpt::PathString::FromNative(static_cast<mpt::os_path>(basename));
		if(!fileNameExtension.empty())
			m_path += P_(".") + fileNameExtension;
	}

	mpt::PathString GetPathname() const { return m_path; }

private:
	mpt::PathString m_path;
};

}  // namespace mpt

OPENMPT_NAMESPACE_END
