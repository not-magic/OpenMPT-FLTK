/* SPDX-License-Identifier: BSL-1.0 OR BSD-3-Clause */

#ifndef MPT_FS_COMMON_DIRECTORIES_HPP
#define MPT_FS_COMMON_DIRECTORIES_HPP



#include "mpt/base/detect.hpp"
#include "mpt/base/namespace.hpp"
#include "mpt/fs/fs.hpp"
#include "mpt/path/native_path.hpp"

#include <cstdlib>
#include <filesystem>
#include <system_error>



namespace mpt {
inline namespace MPT_INLINE_NS {



class common_directories {

public:

	static inline mpt::native_path get_application_directory() {
		std::error_code ec;
		const std::filesystem::path exe = std::filesystem::read_symlink("/proc/self/exe", ec);
		if (ec) {
			return mpt::native_path();
		}
		return mpt::native_path::FromNative(mpt::os_path(exe.parent_path().c_str())) + MPT_NATIVE_PATH("/");
	}

	static inline mpt::native_path get_temp_directory() {
		std::error_code ec;
		const std::filesystem::path temp = std::filesystem::temp_directory_path(ec);
		if (ec) {
			return get_application_directory();
		}
		return mpt::native_path::FromNative(mpt::os_path(temp.c_str())) + MPT_NATIVE_PATH("/");
	}

	static inline mpt::native_path get_home_directory() {
		const char * home = std::getenv("HOME");
		if (!home || !*home) {
			return mpt::native_path();
		}
		return mpt::native_path::FromNative(mpt::os_path(home)) + MPT_NATIVE_PATH("/");
	}

	// XDG_CONFIG_HOME, or ~/.config
	static inline mpt::native_path get_config_directory() {
		const char * config = std::getenv("XDG_CONFIG_HOME");
		if (config && *config) {
			return mpt::native_path::FromNative(mpt::os_path(config)) + MPT_NATIVE_PATH("/");
		}
		return get_home_directory() + MPT_NATIVE_PATH(".config/");
	}

}; // class common_directories



} // namespace MPT_INLINE_NS
} // namespace mpt



#endif // MPT_FS_COMMON_DIRECTORIES_HPP
