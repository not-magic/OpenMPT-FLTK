/*
 * mptFileTemporaryExt.h
 * ---------------------
 * Purpose: Unique temporary path names, which upstream only provides for the Windows tracker build.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */


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
