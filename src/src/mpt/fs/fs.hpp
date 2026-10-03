/* SPDX-License-Identifier: BSL-1.0 OR BSD-3-Clause */

#ifndef MPT_FS_FS_HPP
#define MPT_FS_FS_HPP



#include "mpt/base/detect.hpp"
#include "mpt/base/namespace.hpp"
#include "mpt/path/native_path.hpp"
#include "mpt/path/os_path_long.hpp"

#include <filesystem>
#include <system_error>



namespace mpt {
inline namespace MPT_INLINE_NS {



template <typename Tpath>
class fs;



template <>
class fs<mpt::native_path> {

public:

	// Verify if this path represents a valid directory on the file system.
	bool is_directory(const mpt::native_path & path) {
		std::error_code ec;
		return std::filesystem::is_directory(std::filesystem::path(path.AsNative().c_str()), ec);
	}

	// Verify if this path exists and is a file on the file system.
	bool is_file(const mpt::native_path & path) {
		std::error_code ec;
		const std::filesystem::path fspath(path.AsNative().c_str());
		return std::filesystem::exists(fspath, ec) && !std::filesystem::is_directory(fspath, ec);
	}

	// Verify that a path exists (no matter what type)
	bool exists(const mpt::native_path & path) {
		std::error_code ec;
		return std::filesystem::exists(std::filesystem::path(path.AsNative().c_str()), ec);
	}

	mpt::native_path absolute(const mpt::native_path & path) {
		std::error_code ec;
		const std::filesystem::path result = std::filesystem::absolute(std::filesystem::path(path.AsNative().c_str()), ec);
		if (ec) {
			return path;
		}
		return mpt::native_path::FromNative(mpt::os_path(result.c_str()));
	}

	// Deletes a complete directory tree. Handle with EXTREME care.
	// Returns false if any file could not be removed. path must be absolute.
	bool delete_tree(const mpt::native_path & path) {
		if (path.AsNative().empty()) {
			return false;
		}
		const std::filesystem::path fspath(path.AsNative().c_str());
		if (!fspath.is_absolute()) {
			return false;
		}
		std::error_code ec;
		if (!std::filesystem::exists(fspath, ec)) {
			return true;
		}
		if (!std::filesystem::is_directory(fspath, ec)) {
			return false;
		}
		std::filesystem::remove_all(fspath, ec);
		return !ec;
	}

}; // class fs



using native_fs = fs<mpt::native_path>;



} // namespace MPT_INLINE_NS
} // namespace mpt



#endif // MPT_FS_FS_HPP
